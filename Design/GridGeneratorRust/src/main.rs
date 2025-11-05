use clap::Parser;
use rand::seq::SliceRandom;
use rand::Rng;
use serde::{Deserialize, Serialize};
use std::collections::VecDeque;
use std::fs;
use std::path::PathBuf;

#[derive(Parser, Debug)]
#[command(author, version, about, long_about = None)]
struct Args {
    /// Input YAML file containing word lists
    #[arg(short, long)]
    input: PathBuf,

    /// Disable progress indication
    #[arg(short, long)]
    silent: bool,

    /// Maximum attempts to find optimal solution
    #[arg(long, default_value_t = 1000)]
    max_attempts: usize,
}

#[derive(Debug, Deserialize, Serialize)]
struct WordLists {
    horizontal: Vec<String>,
    vertical: Vec<String>,
}

#[derive(Debug, Clone)]
struct Grid {
    cells: Vec<Vec<Option<char>>>,
    width: usize,
    height: usize,
}

#[derive(Debug, Clone)]
struct PlacedWord {
    word: String,
    start_row: usize,
    start_col: usize,
    direction: Direction,
}

#[derive(Debug, Clone, Copy, PartialEq)]
enum Direction {
    Horizontal, // right-to-left
    Vertical,   // bottom-to-top
}

#[derive(Debug, Clone)]
struct Intersection {
    h_word_idx: usize,
    v_word_idx: usize,
    h_char_idx: usize,
    v_char_idx: usize,
    character: char,
}

#[derive(Debug, Clone)]
struct PlacementCandidate {
    word_idx: usize,
    direction: Direction,
    row: usize,
    col: usize,
    score: f64,
    intersections: Vec<usize>, // indices of intersections this placement would create
}

#[derive(Debug, Clone)]
struct ConstraintState {
    placed_words: Vec<PlacedWord>,
    available_intersections: Vec<Intersection>,
    remaining_h_words: Vec<usize>,
    remaining_v_words: Vec<usize>,
}

impl Grid {
    fn new(width: usize, height: usize) -> Self {
        Grid {
            cells: vec![vec![None; width]; height],
            width,
            height,
        }
    }

    fn can_place_word(&self, word: &str, row: usize, col: usize, direction: Direction) -> bool {
        let chars: Vec<char> = word.chars().collect();
        
        match direction {
            Direction::Horizontal => {
                // Check if word fits (right-to-left)
                if col + 1 < chars.len() {
                    return false;
                }
                let start_col = col + 1 - chars.len();
                
                // Check each position
                for (i, &ch) in chars.iter().enumerate() {
                    let c = start_col + i;
                    if let Some(existing) = self.cells[row][c] {
                        if existing != ch {
                            return false;
                        }
                    }
                }
                true
            }
            Direction::Vertical => {
                // Check if word fits (bottom-to-top)
                if row + 1 < chars.len() {
                    return false;
                }
                let start_row = row + 1 - chars.len();
                
                // Check each position
                for (i, &ch) in chars.iter().enumerate() {
                    let r = start_row + i;
                    if let Some(existing) = self.cells[r][col] {
                        if existing != ch {
                            return false;
                        }
                    }
                }
                true
            }
        }
    }

    fn place_word(&mut self, word: &str, row: usize, col: usize, direction: Direction) -> bool {
        if !self.can_place_word(word, row, col, direction) {
            return false;
        }

        let chars: Vec<char> = word.chars().collect();
        
        match direction {
            Direction::Horizontal => {
                let start_col = col + 1 - chars.len();
                for (i, &ch) in chars.iter().enumerate() {
                    self.cells[row][start_col + i] = Some(ch);
                }
            }
            Direction::Vertical => {
                let start_row = row + 1 - chars.len();
                for (i, &ch) in chars.iter().enumerate() {
                    self.cells[start_row + i][col] = Some(ch);
                }
            }
        }
        true
    }

    fn calculate_used_area(&self) -> (usize, usize, usize, usize) {
        let mut min_row = self.height;
        let mut max_row = 0;
        let mut min_col = self.width;
        let mut max_col = 0;
        let mut has_content = false;

        for (r, row) in self.cells.iter().enumerate() {
            for (c, cell) in row.iter().enumerate() {
                if cell.is_some() {
                    has_content = true;
                    min_row = min_row.min(r);
                    max_row = max_row.max(r);
                    min_col = min_col.min(c);
                    max_col = max_col.max(c);
                }
            }
        }

        if has_content {
            (min_row, max_row, min_col, max_col)
        } else {
            (0, 0, 0, 0)
        }
    }

    fn get_used_dimensions(&self) -> (usize, usize) {
        let (min_row, max_row, min_col, max_col) = self.calculate_used_area();
        (max_row - min_row + 1, max_col - min_col + 1)
    }

    fn compact(&mut self) -> (usize, usize) {
        let (min_row, max_row, min_col, max_col) = self.calculate_used_area();
        
        // Create a new compacted grid
        let new_height = max_row - min_row + 1;
        let new_width = max_col - min_col + 1;
        let mut new_cells = vec![vec![None; new_width]; new_height];
        
        for r in 0..new_height {
            for c in 0..new_width {
                new_cells[r][c] = self.cells[min_row + r][min_col + c];
            }
        }
        
        self.cells = new_cells;
        self.width = new_width;
        self.height = new_height;
        
        (min_row, min_col) // Return offset for updating word positions
    }

    fn try_remove_empty_rows_cols(&mut self) -> bool {
        let mut changed = false;
        
        // Try to remove empty rows
        let mut row = 0;
        while row < self.height {
            let row_empty = (0..self.width).all(|c| self.cells[row][c].is_none());
            if row_empty {
                self.cells.remove(row);
                self.height -= 1;
                changed = true;
            } else {
                row += 1;
            }
        }
        
        // Try to remove empty columns
        let mut col = 0;
        while col < self.width {
            let col_empty = (0..self.height).all(|r| self.cells[r][col].is_none());
            if col_empty {
                for row in &mut self.cells {
                    row.remove(col);
                }
                self.width -= 1;
                changed = true;
            } else {
                col += 1;
            }
        }
        
        changed
    }

    fn print(&self) {
        let (min_row, max_row, min_col, max_col) = self.calculate_used_area();
        
        for r in min_row..=max_row {
            for c in min_col..=max_col {
                match self.cells[r][c] {
                    Some(ch) => print!("{} ", ch),
                    None => print!(". "),
                }
            }
            println!();
        }
    }
}

struct WordSearchGenerator {
    horizontal_words: Vec<String>,
    vertical_words: Vec<String>,
    silent: bool,
}

impl WordSearchGenerator {
    fn new(word_lists: WordLists, silent: bool) -> Self {
        // Sort words by length (descending) to place longer words first
        let mut horizontal_words = word_lists.horizontal;
        let mut vertical_words = word_lists.vertical;
        horizontal_words.sort_by(|a, b| b.len().cmp(&a.len()));
        vertical_words.sort_by(|a, b| b.len().cmp(&a.len()));
        
        Self {
            horizontal_words,
            vertical_words,
            silent,
        }
    }

    fn find_all_intersections(&self) -> Vec<Intersection> {
        let mut intersections = Vec::new();
        
        for (h_idx, h_word) in self.horizontal_words.iter().enumerate() {
            for (v_idx, v_word) in self.vertical_words.iter().enumerate() {
                let h_chars: Vec<char> = h_word.chars().collect();
                let v_chars: Vec<char> = v_word.chars().collect();
                
                for (h_char_idx, &h_char) in h_chars.iter().enumerate() {
                    for (v_char_idx, &v_char) in v_chars.iter().enumerate() {
                        if h_char == v_char {
                            intersections.push(Intersection {
                                h_word_idx: h_idx,
                                v_word_idx: v_idx,
                                h_char_idx,
                                v_char_idx,
                                character: h_char,
                            });
                        }
                    }
                }
            }
        }
        
        // Sort intersections by potential value (prefer common letters, center positions)
        intersections.sort_by(|a, b| {
            let a_score = self.score_intersection_potential(a);
            let b_score = self.score_intersection_potential(b);
            b_score.partial_cmp(&a_score).unwrap_or(std::cmp::Ordering::Equal)
        });
        
        intersections
    }

    fn score_intersection_potential(&self, intersection: &Intersection) -> f64 {
        let mut score = 0.0;
        
        // Prefer intersections with longer words
        let h_word_len = self.horizontal_words[intersection.h_word_idx].len() as f64;
        let v_word_len = self.vertical_words[intersection.v_word_idx].len() as f64;
        score += (h_word_len + v_word_len) * 2.0;
        
        // Prefer intersections closer to word centers
        let h_center_distance = (intersection.h_char_idx as f64 - h_word_len / 2.0).abs();
        let v_center_distance = (intersection.v_char_idx as f64 - v_word_len / 2.0).abs();
        score += 20.0 - (h_center_distance + v_center_distance);
        
        // Prefer common letters that appear in multiple words
        let letter_frequency = self.count_letter_frequency(intersection.character);
        score += letter_frequency as f64 * 5.0;
        
        score
    }

    fn count_letter_frequency(&self, letter: char) -> usize {
        let mut count = 0;
        for word in &self.horizontal_words {
            count += word.chars().filter(|&c| c == letter).count();
        }
        for word in &self.vertical_words {
            count += word.chars().filter(|&c| c == letter).count();
        }
        count
    }

    fn calculate_placement_score(&self, grid: &Grid, word: &str, row: usize, col: usize, 
                                direction: Direction, _intersections: &[Intersection]) -> f64 {
        let mut score = 0.0;
        
        // Base score - prefer central placements
        let center_row = grid.height as f64 / 2.0;
        let center_col = grid.width as f64 / 2.0;
        let distance_from_center = ((row as f64 - center_row).powi(2) + 
                                   (col as f64 - center_col).powi(2)).sqrt();
        score += 100.0 - distance_from_center;
        
        // Heavily reward intersections
        let mut intersection_count = 0;
        let chars: Vec<char> = word.chars().collect();
        
        match direction {
            Direction::Horizontal => {
                let start_col = col + 1 - chars.len();
                for (i, &ch) in chars.iter().enumerate() {
                    let c = start_col + i;
                    if let Some(existing) = grid.cells[row][c] {
                        if existing == ch {
                            intersection_count += 1;
                            score += 50.0; // Large bonus for each intersection
                        }
                    }
                }
            }
            Direction::Vertical => {
                let start_row = row + 1 - chars.len();
                for (i, &ch) in chars.iter().enumerate() {
                    let r = start_row + i;
                    if let Some(existing) = grid.cells[r][col] {
                        if existing == ch {
                            intersection_count += 1;
                            score += 50.0; // Large bonus for each intersection
                        }
                    }
                }
            }
        }
        
        // Bonus for word length (longer words get priority)
        score += word.len() as f64 * 2.0;
        
        // Bonus for creating more future intersection opportunities
        score += intersection_count as f64 * 25.0;
        
        score
    }

    fn estimate_grid_size(&self) -> (usize, usize) {
        let max_h_len = self.horizontal_words.iter().map(|w| w.len()).max().unwrap_or(0);
        let max_v_len = self.vertical_words.iter().map(|w| w.len()).max().unwrap_or(0);
        
        // More conservative estimation - account for potential intersections
        let h_chars: usize = self.horizontal_words.iter().map(|w| w.len()).sum();
        let v_chars: usize = self.vertical_words.iter().map(|w| w.len()).sum();
        
        // Assume 10-20% overlap from intersections
        let total_chars = h_chars + v_chars;
        let overlap_factor = 0.85; // Expect 15% reduction from intersections
        let estimated_area = (total_chars as f64 * overlap_factor) as usize;
        let estimated_side = (estimated_area as f64).sqrt() as usize;
        
        // Ensure grid can fit the longest words
        let min_width = max_h_len.max(self.vertical_words.len()).max(10);
        let min_height = max_v_len.max(self.horizontal_words.len()).max(10);
        
        let width = estimated_side.max(min_width);
        let height = estimated_side.max(min_height);
        
        (width, height)
    }


    fn generate_candidates(&self, grid: &Grid, word: &str, direction: Direction, 
                          intersections: &[Intersection]) -> Vec<PlacementCandidate> {
        let mut candidates = Vec::new();
        
        match direction {
            Direction::Horizontal => {
                for row in 0..grid.height {
                    for col in (word.len()-1)..grid.width {
                        if grid.can_place_word(word, row, col, direction) {
                            let score = self.calculate_placement_score(grid, word, row, col, direction, intersections);
                            candidates.push(PlacementCandidate {
                                word_idx: 0, // Will be set by caller
                                direction,
                                row,
                                col,
                                score,
                                intersections: Vec::new(), // Will be computed later if needed
                            });
                        }
                    }
                }
            }
            Direction::Vertical => {
                for row in (word.len()-1)..grid.height {
                    for col in 0..grid.width {
                        if grid.can_place_word(word, row, col, direction) {
                            let score = self.calculate_placement_score(grid, word, row, col, direction, intersections);
                            candidates.push(PlacementCandidate {
                                word_idx: 0, // Will be set by caller
                                direction,
                                row,
                                col,
                                score,
                                intersections: Vec::new(),
                            });
                        }
                    }
                }
            }
        }
        
        // Sort by score (highest first)
        candidates.sort_by(|a, b| b.score.partial_cmp(&a.score).unwrap_or(std::cmp::Ordering::Equal));
        
        // Take top candidates to avoid exponential explosion
        candidates.truncate(50);
        candidates
    }

    fn generate_intersection_first(&self, width: usize, height: usize, max_attempts: usize) -> Option<(Grid, Vec<PlacedWord>)> {
        let intersections = self.find_all_intersections();
        let mut best_solution: Option<(Grid, Vec<PlacedWord>)> = None;
        let mut best_score = f64::NEG_INFINITY;

        if !self.silent {
            println!("Intersection-first algorithm: {} intersections", intersections.len());
        }

        for attempt in 0..max_attempts {
            if !self.silent && attempt % 25 == 0 {
                println!("Intersection-first attempt {}/{}", attempt + 1, max_attempts);
            }

            let mut grid = Grid::new(width, height);
            let mut placed_words = Vec::new();
            let mut used_h_words = vec![false; self.horizontal_words.len()];
            let mut used_v_words = vec![false; self.vertical_words.len()];
            let mut rng = rand::thread_rng();

            // Phase 1: Force high-value intersections
            let mut intersections_copy = intersections.clone();
            intersections_copy.shuffle(&mut rng);
            
            let mut forced_intersections = 0;
            for intersection in intersections_copy.iter().take(3) { // Try top 3 intersections
                if used_h_words[intersection.h_word_idx] || used_v_words[intersection.v_word_idx] {
                    continue;
                }

                let h_word = &self.horizontal_words[intersection.h_word_idx];
                let v_word = &self.vertical_words[intersection.v_word_idx];

                // Try to place both words at their intersection
                let center_row = height / 2;
                let center_col = width / 2;
                
                // Calculate positions for intersection
                let h_row = center_row;
                let h_col = center_col + intersection.h_char_idx;
                let v_row = center_row + intersection.v_char_idx;
                let v_col = center_col;

                if h_col < width && v_row < height &&
                   grid.can_place_word(h_word, h_row, h_col, Direction::Horizontal) &&
                   grid.can_place_word(v_word, v_row, v_col, Direction::Vertical) {
                    
                    grid.place_word(h_word, h_row, h_col, Direction::Horizontal);
                    grid.place_word(v_word, v_row, v_col, Direction::Vertical);
                    
                    placed_words.push(PlacedWord {
                        word: h_word.clone(),
                        start_row: h_row,
                        start_col: h_col + 1 - h_word.len(),
                        direction: Direction::Horizontal,
                    });
                    
                    placed_words.push(PlacedWord {
                        word: v_word.clone(),
                        start_row: v_row + 1 - v_word.len(),
                        start_col: v_col,
                        direction: Direction::Vertical,
                    });
                    
                    used_h_words[intersection.h_word_idx] = true;
                    used_v_words[intersection.v_word_idx] = true;
                    forced_intersections += 1;
                }
            }

            // Phase 2: Place remaining words optimally
            let mut success = true;
            
            // Place remaining horizontal words
            for (h_idx, h_word) in self.horizontal_words.iter().enumerate() {
                if used_h_words[h_idx] { continue; }
                
                let candidates = self.generate_candidates(&grid, h_word, Direction::Horizontal, &intersections);
                let mut placed = false;
                
                for candidate in candidates.iter().take(5) {
                    if grid.place_word(h_word, candidate.row, candidate.col, candidate.direction) {
                        placed_words.push(PlacedWord {
                            word: h_word.clone(),
                            start_row: candidate.row,
                            start_col: candidate.col + 1 - h_word.len(),
                            direction: Direction::Horizontal,
                        });
                        placed = true;
                        break;
                    }
                }
                
                if !placed {
                    success = false;
                    break;
                }
            }

            // Place remaining vertical words
            if success {
                for (v_idx, v_word) in self.vertical_words.iter().enumerate() {
                    if used_v_words[v_idx] { continue; }
                    
                    let candidates = self.generate_candidates(&grid, v_word, Direction::Vertical, &intersections);
                    let mut placed = false;
                    
                    for candidate in candidates.iter().take(5) {
                        if grid.place_word(v_word, candidate.row, candidate.col, candidate.direction) {
                            placed_words.push(PlacedWord {
                                word: v_word.clone(),
                                start_row: candidate.row + 1 - v_word.len(),
                                start_col: candidate.col,
                                direction: Direction::Vertical,
                            });
                            placed = true;
                            break;
                        }
                    }
                    
                    if !placed {
                        success = false;
                        break;
                    }
                }
            }

            if success {
                let (used_height, used_width) = grid.get_used_dimensions();
                let area = used_height * used_width;
                
                // Enhanced scoring for intersection-first approach
                let compactness_score = 2000.0 / (area as f64);
                let square_diff = (used_height as i32 - used_width as i32).abs() as f64;
                let squareness_score = 200.0 / (1.0 + square_diff);
                let intersection_bonus = (forced_intersections + self.count_total_intersections(&grid, &placed_words)) as f64 * 25.0;
                
                let total_score = compactness_score + squareness_score + intersection_bonus;
                
                if total_score > best_score {
                    best_score = total_score;
                    best_solution = Some((grid, placed_words));
                    
                    if !self.silent {
                        println!("Intersection-first solution: area {} ({}x{}), intersections: {}, score: {:.2}", 
                                area, used_height, used_width, forced_intersections, total_score);
                    }
                }
            }
        }

        best_solution
    }

    fn count_total_intersections(&self, grid: &Grid, placed_words: &[PlacedWord]) -> usize {
        placed_words.iter()
            .map(|word| self.count_intersections(grid, word))
            .sum()
    }

    fn generate_simulated_annealing(&self, initial_solution: (Grid, Vec<PlacedWord>), iterations: usize) -> (Grid, Vec<PlacedWord>) {
        let mut current_solution = initial_solution;
        let mut best_solution = current_solution.clone();
        let mut best_score = self.evaluate_solution(&current_solution.0, &current_solution.1);
        let mut temperature = 1000.0;
        let cooling_rate = 0.95;
        let mut rng = rand::thread_rng();

        if !self.silent {
            println!("Starting simulated annealing with {} iterations", iterations);
        }

        for iteration in 0..iterations {
            if iteration % 50 == 0 {
                temperature *= cooling_rate;
            }

            // Generate a neighbor solution by slightly moving one word
            let mut new_solution = current_solution.clone();
            if self.try_optimize_single_word(&mut new_solution.0, &mut new_solution.1, &mut rng) {
                let new_score = self.evaluate_solution(&new_solution.0, &new_solution.1);
                let current_score = self.evaluate_solution(&current_solution.0, &current_solution.1);
                
                // Accept if better, or with probability if worse
                let delta = new_score - current_score;
                if delta > 0.0 || rng.gen::<f64>() < (delta / temperature).exp() {
                    current_solution = new_solution;
                    
                    if new_score > best_score {
                        best_score = new_score;
                        best_solution = current_solution.clone();
                        
                        if !self.silent && iteration % 100 == 0 {
                            let (h, w) = best_solution.0.get_used_dimensions();
                            println!("SA iteration {}: new best area {} ({}x{}), score: {:.2}", 
                                   iteration, h * w, h, w, best_score);
                        }
                    }
                }
            }
        }

        best_solution
    }

    fn try_optimize_single_word(&self, grid: &mut Grid, placed_words: &mut Vec<PlacedWord>, rng: &mut impl Rng) -> bool {
        if placed_words.is_empty() { return false; }
        
        let word_idx = rng.gen_range(0..placed_words.len());
        let word = &placed_words[word_idx];
        
        // Remove the word temporarily
        self.remove_word_from_grid(grid, word);
        let removed_word = placed_words.remove(word_idx);
        
        // Try to place it in a better position
        let candidates = self.generate_candidates(grid, &removed_word.word, removed_word.direction, &[]);
        
        for candidate in candidates.iter().take(5) {
            if grid.place_word(&removed_word.word, candidate.row, candidate.col, candidate.direction) {
                placed_words.push(PlacedWord {
                    word: removed_word.word.clone(),
                    start_row: match candidate.direction {
                        Direction::Horizontal => candidate.row,
                        Direction::Vertical => candidate.row + 1 - removed_word.word.len(),
                    },
                    start_col: match candidate.direction {
                        Direction::Horizontal => candidate.col + 1 - removed_word.word.len(),
                        Direction::Vertical => candidate.col,
                    },
                    direction: candidate.direction,
                });
                return true;
            }
        }
        
        // If no better position found, put it back in original position
        if grid.place_word(&removed_word.word, 
                          match removed_word.direction {
                              Direction::Horizontal => removed_word.start_row,
                              Direction::Vertical => removed_word.start_row + removed_word.word.len() - 1,
                          },
                          match removed_word.direction {
                              Direction::Horizontal => removed_word.start_col + removed_word.word.len() - 1,
                              Direction::Vertical => removed_word.start_col,
                          },
                          removed_word.direction) {
            placed_words.push(removed_word);
        }
        
        false
    }

    fn remove_word_from_grid(&self, grid: &mut Grid, word: &PlacedWord) {
        let chars: Vec<char> = word.word.chars().collect();
        
        match word.direction {
            Direction::Horizontal => {
                for (i, _) in chars.iter().enumerate() {
                    let col = word.start_col + i;
                    if col < grid.width {
                        grid.cells[word.start_row][col] = None;
                    }
                }
            }
            Direction::Vertical => {
                for (i, _) in chars.iter().enumerate() {
                    let row = word.start_row + i;
                    if row < grid.height {
                        grid.cells[row][word.start_col] = None;
                    }
                }
            }
        }
    }

    fn evaluate_solution(&self, grid: &Grid, placed_words: &[PlacedWord]) -> f64 {
        let (used_height, used_width) = grid.get_used_dimensions();
        let area = used_height * used_width;
        let compactness_score = 2000.0 / (area as f64);
        let square_diff = (used_height as i32 - used_width as i32).abs() as f64;
        let squareness_score = 200.0 / (1.0 + square_diff);
        let intersection_count = self.count_total_intersections(grid, placed_words);
        let intersection_bonus = intersection_count as f64 * 25.0;
        
        compactness_score + squareness_score + intersection_bonus
    }

    fn generate_optimized(&self, width: usize, height: usize, max_attempts: usize) -> Option<(Grid, Vec<PlacedWord>)> {
        let intersections = self.find_all_intersections();
        let mut best_solution: Option<(Grid, Vec<PlacedWord>)> = None;
        let mut best_score = f64::NEG_INFINITY;

        if !self.silent {
            println!("Found {} potential intersections", intersections.len());
        }

        for attempt in 0..max_attempts {
            if !self.silent && attempt % 50 == 0 {
                println!("Optimization attempt {}/{}", attempt + 1, max_attempts);
            }

            let mut grid = Grid::new(width, height);
            let mut placed_words = Vec::new();
            let mut remaining_h: Vec<_> = (0..self.horizontal_words.len()).collect();
            let mut remaining_v: Vec<_> = (0..self.vertical_words.len()).collect();
            let mut rng = rand::thread_rng();
            
            // Shuffle to try different orderings
            remaining_h.shuffle(&mut rng);
            remaining_v.shuffle(&mut rng);

            let mut success = true;
            let mut placement_queue = VecDeque::new();
            
            // Build initial placement queue with alternating word types for better intersection opportunities
            let mut h_iter = remaining_h.iter();
            let mut v_iter = remaining_v.iter();
            
            loop {
                match (h_iter.next(), v_iter.next()) {
                    (Some(&h_idx), Some(&v_idx)) => {
                        placement_queue.push_back((h_idx, Direction::Horizontal));
                        placement_queue.push_back((v_idx, Direction::Vertical));
                    }
                    (Some(&h_idx), None) => {
                        placement_queue.push_back((h_idx, Direction::Horizontal));
                    }
                    (None, Some(&v_idx)) => {
                        placement_queue.push_back((v_idx, Direction::Vertical));
                    }
                    (None, None) => break,
                }
            }

            // Place words using intelligent candidate selection
            while let Some((word_idx, direction)) = placement_queue.pop_front() {
                let word = match direction {
                    Direction::Horizontal => &self.horizontal_words[word_idx],
                    Direction::Vertical => &self.vertical_words[word_idx],
                };

                let candidates = self.generate_candidates(&grid, word, direction, &intersections);
                
                let mut placed = false;
                // Try the best candidates first, with some randomization
                let try_count = (candidates.len().min(10)).max(1);
                for i in 0..try_count {
                    let candidate_idx = if i < 3 { i } else { rng.gen_range(0..candidates.len()) };
                    if let Some(candidate) = candidates.get(candidate_idx) {
                        if grid.place_word(word, candidate.row, candidate.col, candidate.direction) {
                            placed_words.push(PlacedWord {
                                word: word.clone(),
                                start_row: match candidate.direction {
                                    Direction::Horizontal => candidate.row,
                                    Direction::Vertical => candidate.row + 1 - word.len(),
                                },
                                start_col: match candidate.direction {
                                    Direction::Horizontal => candidate.col + 1 - word.len(),
                                    Direction::Vertical => candidate.col,
                                },
                                direction: candidate.direction,
                            });
                            placed = true;
                            break;
                        }
                    }
                }

                if !placed {
                    success = false;
                    break;
                }
            }

            if success {
                let (used_height, used_width) = grid.get_used_dimensions();
                let area = used_height * used_width;
                let square_diff = (used_height as i32 - used_width as i32).abs() as f64;
                
                // Enhanced scoring that heavily favors compactness and squareness
                let compactness_score = 1000.0 / (area as f64);
                let squareness_score = 100.0 / (1.0 + square_diff);
                let intersection_bonus = placed_words.iter()
                    .map(|word| self.count_intersections(&grid, word))
                    .sum::<usize>() as f64 * 10.0;
                
                let total_score = compactness_score + squareness_score + intersection_bonus;
                
                if total_score > best_score {
                    best_score = total_score;
                    best_solution = Some((grid, placed_words));
                    
                    if !self.silent {
                        println!("Found optimized solution: area {} ({}x{}), score: {:.2}", 
                                area, used_height, used_width, total_score);
                    }
                }
            }
        }

        best_solution
    }

    fn count_intersections(&self, grid: &Grid, word: &PlacedWord) -> usize {
        let mut count = 0;
        let chars: Vec<char> = word.word.chars().collect();
        
        match word.direction {
            Direction::Horizontal => {
                for (i, _) in chars.iter().enumerate() {
                    let col = word.start_col + i;
                    if let Some(_) = grid.cells[word.start_row][col] {
                        // Check if there's a vertical word crossing here
                        for r in 0..grid.height {
                            if r != word.start_row && grid.cells[r][col].is_some() {
                                count += 1;
                                break;
                            }
                        }
                    }
                }
            }
            Direction::Vertical => {
                for (i, _) in chars.iter().enumerate() {
                    let row = word.start_row + i;
                    if let Some(_) = grid.cells[row][word.start_col] {
                        // Check if there's a horizontal word crossing here
                        for c in 0..grid.width {
                            if c != word.start_col && grid.cells[row][c].is_some() {
                                count += 1;
                                break;
                            }
                        }
                    }
                }
            }
        }
        
        count
    }

    fn generate_with_size(&self, width: usize, height: usize, max_attempts: usize) -> Option<(Grid, Vec<PlacedWord>)> {
        let mut rng = rand::thread_rng();
        let mut best_solution: Option<(Grid, Vec<PlacedWord>)> = None;
        let mut best_area = usize::MAX;

        for attempt in 0..max_attempts {
            if !self.silent && attempt % 100 == 0 {
                println!("Attempt {}/{}", attempt + 1, max_attempts);
            }

            let mut grid = Grid::new(width, height);
            let mut placed_words = Vec::new();
            let mut remaining_h = self.horizontal_words.clone();
            let mut remaining_v = self.vertical_words.clone();
            
            remaining_h.shuffle(&mut rng);
            remaining_v.shuffle(&mut rng);

            // Try to place all words
            let mut success = true;
            
            // Place horizontal words (now sorted by length, longest first)
            for word in &remaining_h {
                let mut placed = false;
                let mut attempts = 0;
                
                // First try to find good placement considering existing vertical words
                while !placed && attempts < 150 {
                    let row = rng.gen_range(0..height);
                    let col = rng.gen_range(word.len()-1..width);
                    
                    if grid.place_word(word, row, col, Direction::Horizontal) {
                        placed_words.push(PlacedWord {
                            word: word.clone(),
                            start_row: row,
                            start_col: col + 1 - word.len(),
                            direction: Direction::Horizontal,
                        });
                        placed = true;
                    }
                    attempts += 1;
                }
                
                if !placed {
                    success = false;
                    break;
                }
            }
            
            // Place vertical words (now sorted by length, longest first)
            if success {
                for word in &remaining_v {
                    let mut placed = false;
                    let mut attempts = 0;
                    
                    // Try to place with more attempts for better results
                    while !placed && attempts < 150 {
                        let row = rng.gen_range(word.len()-1..height);
                        let col = rng.gen_range(0..width);
                        
                        if grid.place_word(word, row, col, Direction::Vertical) {
                            placed_words.push(PlacedWord {
                                word: word.clone(),
                                start_row: row + 1 - word.len(),
                                start_col: col,
                                direction: Direction::Vertical,
                            });
                            placed = true;
                        }
                        attempts += 1;
                    }
                    
                    if !placed {
                        success = false;
                        break;
                    }
                }
            }

            if success {
                let (used_height, used_width) = grid.get_used_dimensions();
                let area = used_height * used_width;
                let square_diff = (used_height as i32 - used_width as i32).abs() as usize;
                
                // Improved scoring: heavily weight area reduction, moderately weight squareness
                let score = area * 10 + square_diff * 3;
                
                if score < best_area {
                    best_area = score;
                    best_solution = Some((grid, placed_words));
                    
                    if !self.silent {
                        println!("Found solution with area {} ({}x{}), score: {}", area, used_height, used_width, score);
                    }
                }
            }
        }

        best_solution
    }

    fn generate(&self, max_attempts: usize) -> Option<(Grid, Vec<PlacedWord>)> {
        if !self.silent {
            println!("Generating word search puzzle...");
            println!("Horizontal words: {:?}", self.horizontal_words);
            println!("Vertical words: {:?}", self.vertical_words);
            println!();
        }

        let (initial_width, initial_height) = self.estimate_grid_size();
        
        // Try multiple advanced algorithms in order of sophistication
        let algorithms = [
            ("optimized", 0.6, max_attempts / 5),        // Start very small
            ("intersection-first", 0.7, max_attempts / 5),
            ("optimized", 0.8, max_attempts / 5),
            ("optimized", 1.0, max_attempts / 5),
            ("standard", 1.2, max_attempts / 5),  // Final fallback
        ];
        
        for (algo_type, multiplier, attempts) in algorithms {
            let width = ((initial_width as f64) * multiplier) as usize;
            let height = ((initial_height as f64) * multiplier) as usize;
            
            if !self.silent {
                println!("Trying {} algorithm with grid size: {}x{} ({} attempts)", algo_type, width, height, attempts);
            }
            
            let solution = match algo_type {
                "intersection-first" => self.generate_intersection_first(width, height, attempts),
                "optimized" => self.generate_optimized(width, height, attempts),
                "standard" => self.generate_with_size(width, height, attempts),
                _ => None,
            };
            
            // Apply post-processing optimization to any successful solution
            if let Some((mut grid, mut placed_words)) = solution {
                let original_area = grid.get_used_dimensions().0 * grid.get_used_dimensions().1;
                
                // Phase 1: Apply simulated annealing for local optimization
                if !self.silent {
                    println!("Applying simulated annealing optimization...");
                }
                let (optimized_grid, optimized_words) = self.generate_simulated_annealing((grid, placed_words), 100);
                grid = optimized_grid;
                placed_words = optimized_words;
                
                // Phase 2: Compact the grid
                let (row_offset, col_offset) = grid.compact();
                
                // Update word positions after compaction
                for word in placed_words.iter_mut() {
                    word.start_row = word.start_row.saturating_sub(row_offset);
                    word.start_col = word.start_col.saturating_sub(col_offset);
                }
                
                // Phase 3: Try aggressive compaction
                while grid.try_remove_empty_rows_cols() {
                    // Keep removing until no more empty rows/cols can be removed
                }
                
                let final_area = grid.get_used_dimensions().0 * grid.get_used_dimensions().1;
                if !self.silent {
                    println!("Total optimization: {} -> {} area ({:.1}% reduction)", 
                             original_area, final_area, 
                             100.0 * (1.0 - final_area as f64 / original_area as f64));
                }
                
                return Some((grid, placed_words));
            }
        }

        None
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args = Args::parse();

    // Read and parse the input file
    let input_content = fs::read_to_string(&args.input)?;
    let word_lists: WordLists = serde_yaml::from_str(&input_content)?;

    // Validate input
    if word_lists.horizontal.is_empty() && word_lists.vertical.is_empty() {
        eprintln!("Error: No words provided in input file");
        std::process::exit(1);
    }

    // Create generator and generate puzzle
    let generator = WordSearchGenerator::new(word_lists, args.silent);
    
    match generator.generate(args.max_attempts) {
        Some((grid, placed_words)) => {
            if !args.silent {
                println!("\nSuccessfully generated word search!");
                let (height, width) = grid.get_used_dimensions();
                println!("Final grid size: {}x{} (area: {})", height, width, height * width);
                println!("\nPlaced words:");
                for word in &placed_words {
                    println!("  {} ({:?}) at ({}, {})", 
                             word.word, word.direction, word.start_row, word.start_col);
                }
                println!("\nGrid:");
            }
            grid.print();
        }
        None => {
            eprintln!("Failed to generate word search puzzle. Try increasing --max-attempts or using shorter words.");
            std::process::exit(1);
        }
    }

    Ok(())
}