// SVG path converter for UI display
// Converts OpenType font paths to SVG path data, matching DXF export logic

import * as opentype from 'opentype.js';

// Convert OpenType path to SVG path data string (with Y flip)
export function pathToSVG(path: opentype.Path, offsetX: number, offsetY: number): string {
  return pathToSVGInternal(path, offsetX, offsetY, true);
}

// Convert OpenType path to SVG path data string (without Y flip - use transform instead)
export function pathToSVGNoFlip(path: opentype.Path, offsetX: number, offsetY: number): string {
  return pathToSVGInternal(path, offsetX, offsetY, false);
}

// Internal implementation
function pathToSVGInternal(path: opentype.Path, offsetX: number, offsetY: number, flipY: boolean): string {
  if (!path.commands || path.commands.length === 0) {
    return '';
  }

  // Group commands into separate paths (for letters with holes like 'O', 'A', etc.)
  const pathGroups: Array<Array<{type: string, x: number, y: number, x1?: number, y1?: number, x2?: number, y2?: number}>> = [];
  let currentPath: Array<{type: string, x: number, y: number, x1?: number, y1?: number, x2?: number, y2?: number}> = [];

  for (const command of path.commands) {
    if (command.type === 'M' && currentPath.length > 0) {
      pathGroups.push([...currentPath]);
      currentPath = [];
    }
    if (command.type !== 'Z') {
      currentPath.push(command as any);
    } else {
      // Close path marker - add to current path
      if (currentPath.length > 0) {
        currentPath.push(command as any);
      }
    }
  }
  if (currentPath.length > 0) {
    pathGroups.push(currentPath);
  }

  // Convert each path group to SVG path data
  const svgPaths: string[] = [];
  
  for (const pathGroup of pathGroups) {
    if (pathGroup.length === 0) continue;
    
    let pathData = '';
    let currentX = 0;
    let currentY = 0;
    let startX = 0;
    let startY = 0;

    for (const command of pathGroup) {
      switch (command.type) {
        case 'M': // MoveTo
          currentX = (command as any).x + offsetX;
          currentY = flipY ? (-(command as any).y + offsetY) : ((command as any).y + offsetY);
          startX = currentX;
          startY = currentY;
          pathData += `M ${currentX.toFixed(3)} ${currentY.toFixed(3)} `;
          break;

        case 'L': // LineTo
          currentX = (command as any).x + offsetX;
          currentY = flipY ? (-(command as any).y + offsetY) : ((command as any).y + offsetY);
          pathData += `L ${currentX.toFixed(3)} ${currentY.toFixed(3)} `;
          break;

        case 'C': // Cubic Bezier
          const cp1X = (command as any).x1! + offsetX;
          const cp1Y = flipY ? (-(command as any).y1! + offsetY) : ((command as any).y1! + offsetY);
          const cp2X = (command as any).x2! + offsetX;
          const cp2Y = flipY ? (-(command as any).y2! + offsetY) : ((command as any).y2! + offsetY);
          const endX = (command as any).x + offsetX;
          const endY = flipY ? (-(command as any).y + offsetY) : ((command as any).y + offsetY);
          pathData += `C ${cp1X.toFixed(3)} ${cp1Y.toFixed(3)}, ${cp2X.toFixed(3)} ${cp2Y.toFixed(3)}, ${endX.toFixed(3)} ${endY.toFixed(3)} `;
          currentX = endX;
          currentY = endY;
          break;

        case 'Q': // Quadratic Bezier
          const qcpX = (command as any).x1! + offsetX;
          const qcpY = flipY ? (-(command as any).y1! + offsetY) : ((command as any).y1! + offsetY);
          const qendX = (command as any).x + offsetX;
          const qendY = flipY ? (-(command as any).y + offsetY) : ((command as any).y + offsetY);
          pathData += `Q ${qcpX.toFixed(3)} ${qcpY.toFixed(3)}, ${qendX.toFixed(3)} ${qendY.toFixed(3)} `;
          currentX = qendX;
          currentY = qendY;
          break;

        case 'Z': // ClosePath
          pathData += 'Z ';
          break;
      }
    }

    if (pathData.trim()) {
      svgPaths.push(pathData.trim());
    }
  }

  return svgPaths.join(' ');
}

