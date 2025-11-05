# Letter Grid Optimizer

An attempt to find an optimal layout of letters given a set of desired words with some constraints which minimizes grid size. It does not work well and was not used for the grids in the produced wordy watch.

## Problem Description

This program solves a constrained crossword puzzle optimization problem with the following requirements:

### Words to Place
- **Horizontal words**: zero, one, two, three, four, five, six, seven, eight, nine, ten, eleven, twelve, thir, fif, teen, twenty
- **Vertical words**: hundred, ten, twenty, thirty, forty, fifty, five

### Constraints
1. The word "teen" (horizontal) must be the bottom-right-most horizontal word in the grid
2. The word "five" (vertical) must be the bottom-right-most vertical word in the grid
3. Minimize grid size by maximizing letter reuse through word intersections

## Features

- **Efficient optimization**: Uses a greedy algorithm with intelligent scoring to find good solutions quickly
- **Constraint validation**: Ensures all solutions satisfy the positioning requirements
- **Intersection detection**: Automatically finds and utilizes letter overlaps between words
- **Visual output**: Displays the optimized grid layout with word placements

## Usage

```bash
python crossword_optimizer.py
```

## Example Output

```
Crossword Grid Optimizer
==================================================
Finding optimal arrangement...
Trying different arrangements...
Found 2 possible intersections between 'teen' and 'five'
Trying intersection at teen[1] = five[3]
New best solution found! Grid size: 220 (constraints satisfied)
Trying intersection at teen[2] = five[3]
New best solution found! Grid size: 209 (constraints satisfied)
...

Best solution found with grid size: 209

Optimal Grid Layout:
------------------------------
Grid size: 19 x 11 = 209
✓ Constraints satisfied

 0: ...........three...
 1: ............u......
 2: ..........f.nine...
 3: ..........o.d......
 4: ........zeror......
 5: ........tttfeleven.
 6: ........hwyidtwelve
 7: ....thirietffive...
 8: ...fifournetiseven.
 9: twoneighttnyvsix...
10: tentwentyyteen.....
```

## Algorithm Details

The optimizer uses a multi-step approach:

1. **Constraint-based initialization**: Places the constrained words ("teen" and "five") first
2. **Intersection analysis**: Identifies potential letter overlaps between horizontal and vertical words
3. **Greedy placement**: Uses a scoring system to place remaining words optimally:
   - Prioritizes intersections (10 points per shared letter)
   - Prefers compact arrangements (distance penalty)
   - Maintains constraint satisfaction
4. **Validation**: Ensures all solutions meet the positioning requirements

## Code Structure

- `WordPlacement`: Represents a word's position and orientation in the grid
- `CrosswordGrid`: Manages the grid state and word placements
- `CrosswordOptimizer`: Implements the optimization algorithm
- Constraint validation ensures solution correctness

## Performance

The program typically finds optimal or near-optimal solutions in seconds by:
- Limiting the search space to promising configurations
- Using greedy heuristics instead of exhaustive search
- Focusing on high-value intersections first

## Customization

To solve similar problems with different words or constraints:
1. Modify the word lists in `CrosswordGrid.__init__()`
2. Update constraint checking in `validate_constraints()`
3. Adjust the scoring function in `_calculate_placement_score()`
