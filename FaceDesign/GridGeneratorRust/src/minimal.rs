use std::env;
use std::fs;
use std::collections::HashMap;

// Minimal version without external dependencies
// to test if the MSVC requirement comes from dependencies or Rust itself

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let args: Vec<String> = env::args().collect();
    
    if args.len() < 2 {
        eprintln!("Usage: {} <word1> <word2> ...", args[0]);
        std::process::exit(1);
    }
    
    println!("Creating minimal word search with words: {:?}", &args[1..]);
    
    // Simple test grid
    let mut grid = vec![vec!['.'; 10]; 10];
    
    // Place first word horizontally (right-to-left)
    if let Some(word) = args.get(1) {
        let chars: Vec<char> = word.chars().collect();
        let row = 2;
        let start_col = 8;
        
        for (i, &ch) in chars.iter().enumerate() {
            if start_col >= i && start_col - i < 10 {
                grid[row][start_col - i] = ch;
            }
        }
    }
    
    // Place second word vertically (bottom-to-top)
    if let Some(word) = args.get(2) {
        let chars: Vec<char> = word.chars().collect();
        let col = 5;
        let start_row = 7;
        
        for (i, &ch) in chars.iter().enumerate() {
            if start_row >= i && start_row - i < 10 {
                grid[start_row - i][col] = ch;
            }
        }
    }
    
    // Print grid
    for row in &grid {
        for &cell in row {
            print!("{} ", cell);
        }
        println!();
    }
    
    Ok(())
}
