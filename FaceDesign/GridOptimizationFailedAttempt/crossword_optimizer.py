"""
Crossword Style Word Grid Optimizer

This program finds the optimal arrangement of horizontal and vertical words
in a grid to minimize the total grid size while maximizing letter reuse.

Constraints:
- 'teen' (horizontal) must be the bottom-right-most horizontal word
- 'five' (vertical) must be the bottom-right-most vertical word
"""

from typing import List, Tuple, Dict, Set, Optional
from itertools import permutations
import copy

class WordPlacement:
    """Represents a word placement in the grid"""
    def __init__(self, word: str, row: int, col: int, direction: str):
        self.word = word
        self.row = row
        self.col = col
        self.direction = direction  # 'horizontal' or 'vertical'
    
    def get_positions(self) -> List[Tuple[int, int]]:
        """Get all grid positions occupied by this word"""
        positions = []
        if self.direction == 'horizontal':
            for i, char in enumerate(self.word):
                positions.append((self.row, self.col + i))
        else:  # vertical
            for i, char in enumerate(self.word):
                positions.append((self.row + i, self.col))
        return positions
    
    def get_char_at_position(self, row: int, col: int) -> Optional[str]:
        """Get the character at a specific position if this word occupies it"""
        positions = self.get_positions()
        if (row, col) in positions:
            if self.direction == 'horizontal':
                return self.word[col - self.col]
            else:  # vertical
                return self.word[row - self.row]
        return None

class CrosswordGrid:
    """Manages the crossword grid and word placements"""
    
    def __init__(self):
        self.horizontal_words = ["zero", "one", "two", "three", "four", "five", "six", 
                               "seven", "eight", "nine", "ten", "eleven", "twelve", 
                               "thir", "fif", "teen", "twenty"]
        self.vertical_words = ["hundred", "ten", "twenty", "thirty", "forty", "fifty", "five"]
        self.placements: List[WordPlacement] = []
        self.grid: Dict[Tuple[int, int], str] = {}
    
    def clear(self):
        """Clear all placements and grid"""
        self.placements = []
        self.grid = {}
    
    def can_place_word(self, word: str, row: int, col: int, direction: str) -> bool:
        """Check if a word can be placed at the given position"""
        placement = WordPlacement(word, row, col, direction)
        positions = placement.get_positions()
        
        # Check bounds (assuming we allow negative coordinates for now)
        for pos_row, pos_col in positions:
            if pos_row < 0 or pos_col < 0:
                return False
        
        # Check for conflicts with existing placements
        for pos_row, pos_col in positions:
            char = placement.get_char_at_position(pos_row, pos_col)
            if (pos_row, pos_col) in self.grid:
                if self.grid[(pos_row, pos_col)] != char:
                    return False
        
        return True
    
    def place_word(self, word: str, row: int, col: int, direction: str) -> bool:
        """Place a word in the grid if possible"""
        if not self.can_place_word(word, row, col, direction):
            return False
        
        placement = WordPlacement(word, row, col, direction)
        self.placements.append(placement)
        
        # Update grid
        positions = placement.get_positions()
        for pos_row, pos_col in positions:
            char = placement.get_char_at_position(pos_row, pos_col)
            self.grid[(pos_row, pos_col)] = char
        
        return True
    
    def remove_last_placement(self):
        """Remove the last word placement"""
        if not self.placements:
            return
        
        last_placement = self.placements.pop()
        
        # Remove from grid (need to check if other words use the same positions)
        positions_to_remove = last_placement.get_positions()
        for pos_row, pos_col in positions_to_remove:
            # Check if any other placement uses this position
            used_by_other = False
            for placement in self.placements:
                if (pos_row, pos_col) in placement.get_positions():
                    used_by_other = True
                    break
            
            if not used_by_other and (pos_row, pos_col) in self.grid:
                del self.grid[(pos_row, pos_col)]
    
    def get_grid_bounds(self) -> Tuple[int, int, int, int]:
        """Get the bounds of the current grid (min_row, max_row, min_col, max_col)"""
        if not self.grid:
            return 0, 0, 0, 0
        
        rows = [pos[0] for pos in self.grid.keys()]
        cols = [pos[1] for pos in self.grid.keys()]
        
        return min(rows), max(rows), min(cols), max(cols)
    
    def get_grid_size(self) -> int:
        """Get the total grid size (width * height)"""
        min_row, max_row, min_col, max_col = self.get_grid_bounds()
        width = max_col - min_col + 1
        height = max_row - min_row + 1
        return width * height
    
    def validate_constraints(self) -> bool:
        """Validate that the grid satisfies all constraints"""
        teen_placement = None
        five_placement = None
        
        # Find teen and five placements
        for placement in self.placements:
            if placement.word == 'teen' and placement.direction == 'horizontal':
                teen_placement = placement
            elif placement.word == 'five' and placement.direction == 'vertical':
                five_placement = placement
        
        if not teen_placement or not five_placement:
            return False
        
        # Check that 'teen' is the bottom-right-most horizontal word
        teen_end_row = teen_placement.row
        teen_end_col = teen_placement.col + len('teen') - 1
        
        for placement in self.placements:
            if placement.direction == 'horizontal' and placement.word != 'teen':
                word_end_row = placement.row
                word_end_col = placement.col + len(placement.word) - 1
                
                if (word_end_row > teen_end_row or 
                    (word_end_row == teen_end_row and word_end_col > teen_end_col)):
                    return False
        
        # Check that 'five' is the bottom-right-most vertical word
        five_end_row = five_placement.row + len('five') - 1
        five_end_col = five_placement.col
        
        for placement in self.placements:
            if placement.direction == 'vertical' and placement.word != 'five':
                word_end_row = placement.row + len(placement.word) - 1
                word_end_col = placement.col
                
                if (word_end_row > five_end_row or 
                    (word_end_row == five_end_row and word_end_col > five_end_col)):
                    return False
        
        return True
    
    def print_grid(self, style='compact'):
        """Print the current grid with various display options"""
        if not self.grid:
            print("Empty grid")
            return
        
        min_row, max_row, min_col, max_col = self.get_grid_bounds()
        
        print(f"Grid size: {max_col - min_col + 1} x {max_row - min_row + 1} = {self.get_grid_size()}")
        
        # Validate constraints
        if self.validate_constraints():
            print("✓ Constraints satisfied")
        else:
            print("✗ Constraints NOT satisfied")
        
        print()
        
        if style == 'pretty':
            self._print_pretty_grid(min_row, max_row, min_col, max_col)
        elif style == 'boxed':
            self._print_boxed_grid(min_row, max_row, min_col, max_col)
        else:  # compact
            self._print_compact_grid(min_row, max_row, min_col, max_col)
        
        print()
        print("Word placements:")
        for placement in self.placements:
            direction_arrow = "→" if placement.direction == 'horizontal' else "↓"
            print(f"  {placement.word:<8} {direction_arrow} ({placement.row:2d}, {placement.col:2d})")
    
    def _print_compact_grid(self, min_row, max_row, min_col, max_col):
        """Print the grid in compact format"""
        for row in range(min_row, max_row + 1):
            line = ""
            for col in range(min_col, max_col + 1):
                if (row, col) in self.grid:
                    line += self.grid[(row, col)]
                else:
                    line += "·"
            print(f"{row:2d}: {line}")
    
    def _print_pretty_grid(self, min_row, max_row, min_col, max_col):
        """Print the grid with enhanced spacing and formatting"""
        print("    " + "".join(f"{col%10}" for col in range(min_col, max_col + 1)))
        print("    " + "─" * (max_col - min_col + 1))
        
        for row in range(min_row, max_row + 1):
            line = f"{row:2d}│ "
            for col in range(min_col, max_col + 1):
                if (row, col) in self.grid:
                    line += self.grid[(row, col)]
                else:
                    line += "·"
            print(line + " │")
        
        print("    " + "─" * (max_col - min_col + 1))
    
    def _print_boxed_grid(self, min_row, max_row, min_col, max_col):
        """Print the grid with box drawing characters"""
        width = max_col - min_col + 1
        
        # Top border
        print("    ┌" + "─" * width + "┐")
        
        for row in range(min_row, max_row + 1):
            line = f"{row:2d}│ │"
            for col in range(min_col, max_col + 1):
                if (row, col) in self.grid:
                    line = line[:-1] + self.grid[(row, col)] + "│"
                else:
                    line = line[:-1] + "·│"
            print(line)
        
        # Bottom border
        print("    └" + "─" * width + "┘")
    
    def print_solution_summary(self):
        """Print a comprehensive summary of the solution"""
        print("\n" + "=" * 60)
        print("                    SOLUTION SUMMARY")
        print("=" * 60)
        
        if not self.grid:
            print("No solution found!")
            return
        
        min_row, max_row, min_col, max_col = self.get_grid_bounds()
        width = max_col - min_col + 1
        height = max_row - min_row + 1
        
        print(f"Grid dimensions: {width} × {height} = {self.get_grid_size()} cells")
        print(f"Words placed: {len(self.placements)}")
        
        # Count intersections
        total_intersections = 0
        for placement in self.placements:
            for pos in placement.get_positions():
                if pos in self.grid:
                    # Count how many other words use this position
                    count = sum(1 for p in self.placements if pos in p.get_positions())
                    if count > 1:
                        total_intersections += count - 1
        
        print(f"Letter intersections: {total_intersections // 2}")  # Divide by 2 to avoid double counting
        
        # Constraint status
        if self.validate_constraints():
            print("Constraints: ✓ All satisfied")
        else:
            print("Constraints: ✗ Violations detected")
        
        print("\nFor best viewing:")
        print("• Use a monospace font (Consolas, Courier New, or Monaco)")
        print("• Zoom to comfortable size")
        print("• '·' represents empty cells")
        print("• Letters show word intersections")
        
        print("\n" + "─" * 60)
    
    def save_grid_to_file(self, filename="crossword_solution.txt", style='pretty'):
        """Save the grid to a text file for better viewing"""
        with open(filename, 'w', encoding='utf-8') as f:
            # Redirect print output to file
            import sys
            original_stdout = sys.stdout
            sys.stdout = f
            
            try:
                self.print_solution_summary()
                print("\nCROSSWORD GRID LAYOUT:")
                print("=" * 50)
                self.print_grid(style)
                
                print("\nVIEWING INSTRUCTIONS:")
                print("• Open this file in a text editor with monospace font")
                print("• Recommended fonts: Consolas, Courier New, Monaco")
                print("• Adjust zoom for comfortable viewing")
                print("• '·' represents empty cells")
                print("• Letters show where words intersect")
                
            finally:
                sys.stdout = original_stdout
        
        print(f"Grid saved to '{filename}' - open with a monospace font for best viewing!")

class CrosswordOptimizer:
    """Optimizes the crossword grid layout"""
    
    def __init__(self):
        self.grid = CrosswordGrid()
        self.best_grid = None
        self.best_size = float('inf')
    
    def find_intersections(self, h_word: str, v_word: str) -> List[Tuple[int, int]]:
        """Find all possible intersection points between horizontal and vertical words"""
        intersections = []
        for h_idx, h_char in enumerate(h_word):
            for v_idx, v_char in enumerate(v_word):
                if h_char == v_char:
                    intersections.append((h_idx, v_idx))
        return intersections
    
    def solve_with_constraints(self) -> Optional[CrosswordGrid]:
        """
        Solve the crossword puzzle with the given constraints.
        Uses a more efficient approach focusing on key intersections.
        """
        self.best_grid = None
        self.best_size = float('inf')
        
        print("Trying different arrangements...")
        
        # Start with the constrained words: 'teen' horizontal and 'five' vertical
        # Place 'teen' at a reference position
        teen_row, teen_col = 10, 10  # Reference position
        
        # Find intersections between 'teen' and 'five'
        intersections = self.find_intersections('teen', 'five')
        print(f"Found {len(intersections)} possible intersections between 'teen' and 'five'")
        
        configurations_tried = 0
        
        # Try intersecting configurations first (more likely to be optimal)
        for h_idx, v_idx in intersections:
            five_row = teen_row - v_idx
            five_col = teen_col + h_idx
            
            if five_row >= 0 and five_col >= 0:
                print(f"Trying intersection at teen[{h_idx}] = five[{v_idx}]")
                self._try_configuration(teen_row, teen_col, five_row, five_col)
                configurations_tried += 1
        
        # Try a few non-intersecting configurations
        for five_row in range(max(0, teen_row - 3), teen_row + 1):
            for five_col in range(teen_col + 1, teen_col + 3):  # Reduced range
                print(f"Trying non-intersecting: five at ({five_row}, {five_col})")
                self._try_configuration(teen_row, teen_col, five_row, five_col)
                configurations_tried += 1
                if configurations_tried > 10:  # Limit to prevent long runtime
                    break
            if configurations_tried > 10:
                break
        
        print(f"Tried {configurations_tried} configurations")
        return self.best_grid
    
    def _try_configuration(self, teen_row: int, teen_col: int, five_row: int, five_col: int):
        """Try a specific configuration with 'teen' and 'five' positioned"""
        self.grid.clear()
        
        # Place the constrained words first
        if not self.grid.place_word('teen', teen_row, teen_col, 'horizontal'):
            return
        if not self.grid.place_word('five', five_row, five_col, 'vertical'):
            self.grid.clear()
            return
        
        # Get remaining words
        remaining_h = [w for w in self.grid.horizontal_words if w != 'teen']
        remaining_v = [w for w in self.grid.vertical_words if w != 'five']
        
        # Try to place remaining words optimally
        self._place_remaining_words(remaining_h, remaining_v, teen_row, teen_col, five_row, five_col)
    
    def _place_remaining_words(self, h_words: List[str], v_words: List[str], 
                             teen_row: int, teen_col: int, five_row: int, five_col: int):
        """Place remaining words to minimize grid size"""
        
        # Use a greedy approach instead of trying all permutations
        # This is much faster and often finds good solutions
        self._greedy_placement(h_words, v_words, teen_row, teen_col, five_row, five_col)
    
    def _greedy_placement(self, h_words: List[str], v_words: List[str],
                         teen_row: int, teen_col: int, five_row: int, five_col: int):
        """Use greedy approach to place words optimally"""
        
        # Save current state
        saved_placements = copy.deepcopy(self.grid.placements)
        saved_grid = copy.deepcopy(self.grid.grid)
        
        # Place vertical words first (prioritize by potential intersections)
        remaining_v = v_words.copy()
        
        while remaining_v:
            best_word = None
            best_position = None
            max_score = -1
            
            for v_word in remaining_v:
                # Try to find the best position for this word
                for row in range(max(0, teen_row - 15), teen_row + 5):
                    for col in range(max(0, teen_col - 10), teen_col + 15):
                        if (self.grid.can_place_word(v_word, row, col, 'vertical') and
                            self._satisfies_vertical_constraint(v_word, row, col, five_row, five_col)):
                            # Calculate score based on intersections and position
                            temp_placement = WordPlacement(v_word, row, col, 'vertical')
                            score = self._calculate_placement_score(temp_placement, teen_row, teen_col, five_row, five_col)
                            
                            if score > max_score:
                                max_score = score
                                best_word = v_word
                                best_position = (row, col)
            
            if best_word and best_position:
                self.grid.place_word(best_word, best_position[0], best_position[1], 'vertical')
                remaining_v.remove(best_word)
            else:
                # Can't place any more vertical words
                break
        
        # Place horizontal words
        remaining_h = h_words.copy()
        
        while remaining_h:
            best_word = None
            best_position = None
            max_score = -1
            
            for h_word in remaining_h:
                # Try to find the best position for this word
                for row in range(max(0, teen_row - 10), teen_row + 1):  # Must be above or at teen level
                    for col in range(max(0, teen_col - 20), teen_col + 5):
                        if (self.grid.can_place_word(h_word, row, col, 'horizontal') and
                            self._satisfies_horizontal_constraint(h_word, row, col, teen_row, teen_col)):
                            
                            temp_placement = WordPlacement(h_word, row, col, 'horizontal')
                            score = self._calculate_placement_score(temp_placement, teen_row, teen_col, five_row, five_col)
                            
                            if score > max_score:
                                max_score = score
                                best_word = h_word
                                best_position = (row, col)
            
            if best_word and best_position:
                self.grid.place_word(best_word, best_position[0], best_position[1], 'horizontal')
                remaining_h.remove(best_word)
            else:
                # Can't place any more horizontal words
                break
        
        # Check if this is the best solution so far
        current_size = self.grid.get_grid_size()
        if (current_size < self.best_size and len(self.grid.placements) >= 2 and  # At least teen and five
            self.grid.validate_constraints()):
            self.best_size = current_size
            self.best_grid = CrosswordGrid()
            self.best_grid.placements = copy.deepcopy(self.grid.placements)
            self.best_grid.grid = copy.deepcopy(self.grid.grid)
            print(f"New best solution found! Grid size: {current_size} (constraints satisfied)")
        
        # Restore state
        self.grid.placements = saved_placements
        self.grid.grid = saved_grid
    
    def _calculate_placement_score(self, placement: WordPlacement, teen_row: int, teen_col: int,
                                 five_row: int, five_col: int) -> float:
        """Calculate a score for a word placement (higher is better)"""
        score = 0.0
        
        # Bonus for intersections
        intersections = self._count_intersections(placement)
        score += intersections * 10
        
        # Penalty for distance from center
        center_row = (teen_row + five_row) // 2
        center_col = (teen_col + five_col) // 2
        distance = abs(placement.row - center_row) + abs(placement.col - center_col)
        score -= distance * 0.1
        
        # Bonus for compact placement
        if placement.direction == 'vertical':
            # Prefer placing vertical words closer to existing structure
            score += max(0, 5 - abs(placement.col - teen_col))
        else:
            # Prefer placing horizontal words closer to existing structure
            score += max(0, 5 - abs(placement.row - teen_row))
        
        return score
    
    def _satisfies_horizontal_constraint(self, word: str, row: int, col: int, 
                                       teen_row: int, teen_col: int) -> bool:
        """Check if horizontal word placement satisfies the constraint"""
        word_end_row = row
        word_end_col = col + len(word) - 1
        teen_end_col = teen_col + len('teen') - 1
        
        # Word must not be more bottom-right than 'teen'
        return (word_end_row < teen_row or 
                (word_end_row == teen_row and word_end_col <= teen_end_col))
    
    def _satisfies_vertical_constraint(self, word: str, row: int, col: int,
                                     five_row: int, five_col: int) -> bool:
        """Check if vertical word placement satisfies the constraint"""
        word_end_row = row + len(word) - 1
        word_end_col = col
        five_end_row = five_row + len('five') - 1
        five_end_col = five_col
        
        # Word must not be more bottom-right than 'five'
        return (word_end_row < five_end_row or 
                (word_end_row == five_end_row and word_end_col <= five_end_col))
    
    def _try_vertical_arrangement(self, v_words: List[str], h_words: List[str],
                                teen_row: int, teen_col: int, five_row: int, five_col: int):
        """Try a specific arrangement of vertical words"""
        
        # Save current state
        saved_placements = copy.deepcopy(self.grid.placements)
        saved_grid = copy.deepcopy(self.grid.grid)
        
        # Try to place vertical words
        placed_v_words = []
        
        # Find good positions for vertical words by looking for intersections
        for v_word in v_words:
            best_position = None
            max_intersections = -1
            
            # Try positions that could intersect with existing horizontal words
            for placement in self.grid.placements:
                if placement.direction == 'horizontal':
                    intersections = self.find_intersections(placement.word, v_word)
                    
                    for h_idx, v_idx in intersections:
                        v_row = placement.row - v_idx
                        v_col = placement.col + h_idx
                        
                        if (v_row >= 0 and v_col >= 0 and 
                            self.grid.can_place_word(v_word, v_row, v_col, 'vertical')):
                            
                            # Count total intersections this would create
                            temp_placement = WordPlacement(v_word, v_row, v_col, 'vertical')
                            intersection_count = self._count_intersections(temp_placement)
                            
                            if intersection_count > max_intersections:
                                max_intersections = intersection_count
                                best_position = (v_row, v_col)
            
            # If no good intersection found, try placing it in a reasonable position
            if best_position is None:
                # Try positions near existing words
                for row in range(max(0, teen_row - 10), teen_row + 5):
                    for col in range(max(0, teen_col - 10), teen_col + 15):
                        if self.grid.can_place_word(v_word, row, col, 'vertical'):
                            best_position = (row, col)
                            break
                    if best_position:
                        break
            
            if best_position and self.grid.place_word(v_word, best_position[0], best_position[1], 'vertical'):
                placed_v_words.append(v_word)
            else:
                # Restore state and try next arrangement
                self.grid.placements = saved_placements
                self.grid.grid = saved_grid
                return
        
        # Now try to place horizontal words
        self._try_horizontal_arrangement(h_words, teen_row, teen_col, five_row, five_col)
        
        # Restore state for next attempt
        self.grid.placements = saved_placements
        self.grid.grid = saved_grid
    
    def _try_horizontal_arrangement(self, h_words: List[str], teen_row: int, teen_col: int,
                                  five_row: int, five_col: int):
        """Try to place horizontal words optimally"""
        
        for h_word in h_words:
            best_position = None
            max_intersections = -1
            
            # Try positions that intersect with vertical words
            for placement in self.grid.placements:
                if placement.direction == 'vertical':
                    intersections = self.find_intersections(h_word, placement.word)
                    
                    for h_idx, v_idx in intersections:
                        h_row = placement.row + v_idx
                        h_col = placement.col - h_idx
                        
                        if (h_row >= 0 and h_col >= 0 and 
                            self.grid.can_place_word(h_word, h_row, h_col, 'horizontal')):
                            
                            # Check constraint: no horizontal word should be more bottom-right than 'teen'
                            word_end_row = h_row
                            word_end_col = h_col + len(h_word) - 1
                            
                            if (word_end_row < teen_row or 
                                (word_end_row == teen_row and word_end_col <= teen_col + len('teen') - 1)):
                                
                                temp_placement = WordPlacement(h_word, h_row, h_col, 'horizontal')
                                intersection_count = self._count_intersections(temp_placement)
                                
                                if intersection_count > max_intersections:
                                    max_intersections = intersection_count
                                    best_position = (h_row, h_col)
            
            # If no intersection found, try other positions
            if best_position is None:
                for row in range(max(0, teen_row - 10), teen_row + 1):
                    for col in range(max(0, teen_col - 15), teen_col + 5):
                        if (self.grid.can_place_word(h_word, row, col, 'horizontal') and
                            (row < teen_row or (row == teen_row and col + len(h_word) - 1 <= teen_col + len('teen') - 1))):
                            best_position = (row, col)
                            break
                    if best_position:
                        break
            
            if best_position:
                self.grid.place_word(h_word, best_position[0], best_position[1], 'horizontal')
        
        # Check if this is the best solution so far
        current_size = self.grid.get_grid_size()
        if current_size < self.best_size:
            self.best_size = current_size
            self.best_grid = CrosswordGrid()
            self.best_grid.placements = copy.deepcopy(self.grid.placements)
            self.best_grid.grid = copy.deepcopy(self.grid.grid)
    
    def _count_intersections(self, new_placement: WordPlacement) -> int:
        """Count how many intersections a new placement would create"""
        count = 0
        new_positions = new_placement.get_positions()
        
        for pos in new_positions:
            if pos in self.grid.grid:
                # Check if the characters match
                existing_char = self.grid.grid[pos]
                new_char = new_placement.get_char_at_position(pos[0], pos[1])
                if existing_char == new_char:
                    count += 1
        
        return count

def main():
    """Main function to run the crossword optimizer"""
    print("Crossword Grid Optimizer")
    print("=" * 50)
    
    optimizer = CrosswordOptimizer()
    
    print("Finding optimal arrangement...")
    best_solution = optimizer.solve_with_constraints()
    
    if best_solution:
        # Print solution summary
        best_solution.print_solution_summary()
        
        # Show different display styles
        print("\n" + "=" * 60)
        print("                  DISPLAY OPTIONS")
        print("=" * 60)
        
        print("\n1. COMPACT VIEW (default):")
        print("─" * 40)
        best_solution.print_grid('compact')
        
        print("\n2. PRETTY VIEW (with borders and coordinates):")
        print("─" * 50)
        best_solution.print_grid('pretty')
        
        print("\n3. BOXED VIEW (fully bordered):")
        print("─" * 40)
        best_solution.print_grid('boxed')
        
        print("\n" + "=" * 60)
        print("💡 TIP: For best results, use a MONOSPACE font!")
        print("   Recommended fonts: Consolas, Courier New, Monaco, or Fira Code")
        print("=" * 60)
        
        # Save to file for better viewing
        print("\nSaving solution to file...")
        best_solution.save_grid_to_file("crossword_solution.txt", "pretty")
        
    else:
        print("No valid solution found!")

if __name__ == "__main__":
    main()
