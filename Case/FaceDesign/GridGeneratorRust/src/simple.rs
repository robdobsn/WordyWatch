// Simple word search generator without external dependencies
// This version can be compiled with just: rustc src/simple.rs

use std::env;
use std::fs;

struct Grid {
    cells: Vec<Vec<Option<char>>>,
    width: usize,
    height: usize,
}

impl Grid {
    fn new(width: usize, height: usize) -> Self {
        Grid {
            cells: vec![vec![None; width]; height],
            width,
            height,
        }
    }

    fn can_place_horizontal(&self, word: &str, row: usize, col: usize) -> bool {
        let chars: Vec<char> = word.chars().collect();
        if col + 1 < chars.len() {
            return false;
        }
        let start_col = col + 1 - chars.len();
        
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

    fn can_place_vertical(&self, word: &str, row: usize, col: usize) -> bool {
        let chars: Vec<char> = word.chars().collect();
        if row + 1 < chars.len() {
            return false;
        }
        let start_row = row + 1 - chars.len();
        
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

    fn place_horizontal(&mut self, word: &str, row: usize, col: usize) -> bool {
        if !self.can_place_horizontal(word, row, col) {
            return false;
        }
        let chars: Vec<char> = word.chars().collect();
        let start_col = col + 1 - chars.len();
        for (i, &ch) in chars.iter().enumerate() {
            self.cells[row][start_col + i] = Some(ch);
        }
        true
    }

    fn place_vertical(&mut self, word: &str, row: usize, col: usize) -> bool {
        if !self.can_place_vertical(word, row, col) {
            return false;
        }
        let chars: Vec<char> = word.chars().collect();
        let start_row = row + 1 - chars.len();
        for (i, &ch) in chars.iter().enumerate() {
            self.cells[start_row + i][col] = Some(ch);
        }
        true
    }

    fn print(&self) {
        for row in &self.cells {
            for cell in row {
                match cell {
                    Some(ch) => print!("{} ", ch),
                    None => print!(". "),
                }
            }
            println!();
        }
    }
}

fn parse_simple_input(content: &str) -> (Vec<String>, Vec<String>) {
    let lines: Vec<&str> = content.lines().collect();
    let mut horizontal = Vec::new();
    let mut vertical = Vec::new();
    let mut current_section = "";

    for line in lines {
        let line = line.trim();
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        if line == "horizontal:" {
            current_section = "horizontal";
            continue;
        }
        if line == "vertical:" {
            current_section = "vertical";
            continue;
        }
        if line.starts_with("- ") {
            let word = line[2..].trim_matches('"').to_uppercase();
            match current_section {
                "horizontal" => horizontal.push(word),
                "vertical" => vertical.push(word),
                _ => {}
            }
        }
    }

    (horizontal, vertical)
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = env::args().collect();
    
    if args.len() != 2 {
        eprintln!("Usage: {} <input_file>", args[0]);
        eprintln!("Input file should be a simple YAML-like format:");
        eprintln!("horizontal:");
        eprintln!("- \"WORD1\"");
        eprintln!("- \"WORD2\"");
        eprintln!("vertical:");
        eprintln!("- \"WORD3\"");
        eprintln!("- \"WORD4\"");
        std::process::exit(1);
    }

    let content = fs::read_to_string(&args[1])?;
    let (horizontal_words, vertical_words) = parse_simple_input(&content);

    println!("Horizontal words: {:?}", horizontal_words);
    println!("Vertical words: {:?}", vertical_words);

    // Simple grid creation
    let mut grid = Grid::new(20, 20);
    
    // Place some words manually for demonstration
    if let Some(word) = horizontal_words.get(0) {
        grid.place_horizontal(word, 5, 15);
        println!("Placed '{}' horizontally", word);
    }
    
    if let Some(word) = vertical_words.get(0) {
        grid.place_vertical(word, 15, 8);
        println!("Placed '{}' vertically", word);
    }

    if let Some(word) = horizontal_words.get(1) {
        grid.place_horizontal(word, 10, 12);
        println!("Placed '{}' horizontally", word);
    }

    println!("\nGenerated grid:");
    grid.print();

    Ok(())
}
