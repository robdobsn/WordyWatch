#!/usr/bin/env python3
"""
DXF Validation Tool using ezdxf

This tool validates DXF files and provides detailed error reporting
to help debug issues with DXF generation.
"""

import argparse
import sys
import os
from pathlib import Path
import ezdxf
from ezdxf import recover
from ezdxf.lldxf.const import DXFStructureError, DXFValueError
# from ezdxf.lldxf.validator import is_valid_handle  # Not available in this version


def validate_dxf_file(file_path: str) -> dict:
    """
    Validate a DXF file and return detailed results.
    
    Args:
        file_path: Path to the DXF file to validate
        
    Returns:
        Dictionary with validation results
    """
    results = {
        'file_exists': False,
        'file_readable': False,
        'dxf_version': None,
        'is_valid_dxf': False,
        'errors': [],
        'warnings': [],
        'entities_count': 0,
        'layers': [],
        'text_entities': 0,
        'line_entities': 0,
        'polyline_entities': 0,
        'other_entities': 0,
        'sections': [],
        'tables': []
    }
    
    # Check if file exists
    if not os.path.exists(file_path):
        results['errors'].append(f"File does not exist: {file_path}")
        return results
    results['file_exists'] = True
    
    # Check if file is readable
    try:
        with open(file_path, 'rb') as f:
            f.read(1)
        results['file_readable'] = True
    except Exception as e:
        results['errors'].append(f"Cannot read file: {e}")
        return results
    
    # Try to load with ezdxf
    try:
        # First try normal loading
        doc = ezdxf.readfile(file_path)
        results['is_valid_dxf'] = True
        results['dxf_version'] = doc.dxfversion
        print(f"Successfully loaded DXF version: {doc.dxfversion}")
        
    except ezdxf.DXFStructureError as e:
        results['errors'].append(f"DXF Structure Error: {e}")
        print(f"DXF Structure Error: {e}")
        
        # Try recovery mode
        try:
            print("Attempting recovery mode...")
            doc, auditor = recover.readfile(file_path)
            results['is_valid_dxf'] = True
            results['dxf_version'] = doc.dxfversion
            results['warnings'].append("File loaded in recovery mode - some data may be lost")
            print(f"Recovery successful - DXF version: {doc.dxfversion}")
            
            # Report recovery issues
            if auditor.has_errors:
                for error in auditor.errors:
                    results['warnings'].append(f"Recovery error: {error}")
                    print(f"Recovery error: {error}")
                    
        except Exception as recovery_error:
            results['errors'].append(f"Recovery failed: {recovery_error}")
            print(f"Recovery failed: {recovery_error}")
            return results
            
    except Exception as e:
        results['errors'].append(f"Failed to load DXF: {e}")
        print(f"Failed to load DXF: {e}")
        return results
    
    # Analyze the document structure
    try:
        analyze_document(doc, results)
    except Exception as e:
        results['errors'].append(f"Analysis error: {e}")
        print(f"✗ Analysis error: {e}")
    
    return results


def analyze_document(doc, results):
    """Analyze the DXF document structure."""
    
    # Check sections
    for section_name in ['HEADER', 'TABLES', 'BLOCKS', 'ENTITIES', 'OBJECTS']:
        if hasattr(doc, section_name.lower()):
            results['sections'].append(section_name)
    
    # Check tables
    if hasattr(doc, 'tables'):
        for table_name in ['layers', 'styles', 'views', 'ucs', 'appids']:
            if hasattr(doc.tables, table_name):
                results['tables'].append(table_name)
    
    # Analyze entities
    modelspace = doc.modelspace()
    results['entities_count'] = len(modelspace)
    
    for entity in modelspace:
        entity_type = entity.dxftype()
        
        if entity_type == 'TEXT':
            results['text_entities'] += 1
            validate_text_entity(entity, results)
        elif entity_type == 'LINE':
            results['line_entities'] += 1
            validate_line_entity(entity, results)
        elif entity_type == 'LWPOLYLINE':
            results['polyline_entities'] += 1
            validate_polyline_entity(entity, results)
        else:
            results['other_entities'] += 1
    
    # Check layers
    if hasattr(doc, 'layers'):
        for layer in doc.layers:
            results['layers'].append(layer.dxf.name)
    
    # Validate handles
    validate_handles(doc, results)


def validate_text_entity(entity, results):
    """Validate a TEXT entity."""
    try:
        # Check required properties
        if not hasattr(entity.dxf, 'text'):
            results['warnings'].append(f"TEXT entity missing text content: {entity.dxf.handle}")
        
        if not hasattr(entity.dxf, 'insert'):
            results['warnings'].append(f"TEXT entity missing insert point: {entity.dxf.handle}")
        
        # Check text height
        if hasattr(entity.dxf, 'height') and entity.dxf.height <= 0:
            results['warnings'].append(f"TEXT entity has invalid height: {entity.dxf.height}")
            
    except Exception as e:
        results['warnings'].append(f"Error validating TEXT entity {entity.dxf.handle}: {e}")


def validate_line_entity(entity, results):
    """Validate a LINE entity."""
    try:
        # Check start and end points
        if not hasattr(entity.dxf, 'start') or not hasattr(entity.dxf, 'end'):
            results['warnings'].append(f"LINE entity missing start/end points: {entity.dxf.handle}")
            
    except Exception as e:
        results['warnings'].append(f"Error validating LINE entity {entity.dxf.handle}: {e}")


def validate_polyline_entity(entity, results):
    """Validate a LWPOLYLINE entity."""
    try:
        # Check if it has points
        if not hasattr(entity, 'get_points') or len(list(entity.get_points())) == 0:
            results['warnings'].append(f"LWPOLYLINE entity has no points: {entity.dxf.handle}")
            
        # Check if it's closed when it should be
        if hasattr(entity.dxf, 'flags') and entity.dxf.flags & 1:  # Closed flag
            points = list(entity.get_points())
            if len(points) > 0:
                first_point = points[0]
                last_point = points[-1]
                if abs(first_point[0] - last_point[0]) > 1e-6 or abs(first_point[1] - last_point[1]) > 1e-6:
                    results['warnings'].append(f"LWPOLYLINE marked as closed but not actually closed: {entity.dxf.handle}")
                    
    except Exception as e:
        results['warnings'].append(f"Error validating LWPOLYLINE entity {entity.dxf.handle}: {e}")


def validate_handles(doc, results):
    """Validate entity handles."""
    handles = set()
    duplicate_handles = []
    
    try:
        for entity in doc.modelspace():
            handle = entity.dxf.handle
            if handle in handles:
                duplicate_handles.append(handle)
            else:
                handles.add(handle)
            
            # Simple handle validation - handles should be hexadecimal strings
            if not isinstance(handle, str) or not all(c in '0123456789ABCDEF' for c in handle.upper()):
                results['warnings'].append(f"Invalid handle format: {handle}")
    
    except Exception as e:
        results['warnings'].append(f"Error validating handles: {e}")
    
    if duplicate_handles:
        results['errors'].append(f"Duplicate handles found: {duplicate_handles}")


def print_results(results):
    """Print validation results in a formatted way."""
    print("\n" + "="*60)
    print("DXF VALIDATION RESULTS")
    print("="*60)
    
    # Basic file info
    print(f"File exists: {'YES' if results['file_exists'] else 'NO'}")
    print(f"File readable: {'YES' if results['file_readable'] else 'NO'}")
    print(f"Valid DXF: {'YES' if results['is_valid_dxf'] else 'NO'}")
    
    if results['dxf_version']:
        print(f"DXF Version: {results['dxf_version']}")
    
    # Document structure
    print(f"\nSections found: {', '.join(results['sections']) if results['sections'] else 'None'}")
    print(f"Tables found: {', '.join(results['tables']) if results['tables'] else 'None'}")
    print(f"Layers: {', '.join(results['layers']) if results['layers'] else 'None'}")
    
    # Entity counts
    print(f"\nEntity counts:")
    print(f"  Total entities: {results['entities_count']}")
    print(f"  TEXT entities: {results['text_entities']}")
    print(f"  LINE entities: {results['line_entities']}")
    print(f"  LWPOLYLINE entities: {results['polyline_entities']}")
    print(f"  Other entities: {results['other_entities']}")
    
    # Errors
    if results['errors']:
        print(f"\nERRORS ({len(results['errors'])}):")
        for i, error in enumerate(results['errors'], 1):
            print(f"  {i}. {error}")
    else:
        print(f"\nNo errors found")
    
    # Warnings
    if results['warnings']:
        print(f"\nWARNINGS ({len(results['warnings'])}):")
        for i, warning in enumerate(results['warnings'], 1):
            print(f"  {i}. {warning}")
    else:
        print(f"\nNo warnings found")
    
    print("="*60)


def main():
    parser = argparse.ArgumentParser(
        description="Validate DXF files and report detailed errors",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python validate_dxf.py file.dxf
  python validate_dxf.py -v file.dxf
  python validate_dxf.py --output report.txt file.dxf
        """
    )
    
    parser.add_argument(
        'file',
        help='DXF file to validate'
    )
    
    parser.add_argument(
        '-v', '--verbose',
        action='store_true',
        help='Enable verbose output'
    )
    
    parser.add_argument(
        '--output', '-o',
        help='Output file for detailed report'
    )
    
    args = parser.parse_args()
    
    # Check if file exists
    if not os.path.exists(args.file):
        print(f"Error: File '{args.file}' does not exist")
        sys.exit(1)
    
    print(f"Validating DXF file: {args.file}")
    print(f"File size: {os.path.getsize(args.file)} bytes")
    
    # Validate the file
    results = validate_dxf_file(args.file)
    
    # Print results
    print_results(results)
    
    # Save to output file if requested
    if args.output:
        with open(args.output, 'w') as f:
            f.write(f"DXF Validation Report for: {args.file}\n")
            f.write(f"Generated: {__import__('datetime').datetime.now()}\n\n")
            
            f.write("ERRORS:\n")
            for i, error in enumerate(results['errors'], 1):
                f.write(f"{i}. {error}\n")
            
            f.write("\nWARNINGS:\n")
            for i, warning in enumerate(results['warnings'], 1):
                f.write(f"{i}. {warning}\n")
            
            f.write(f"\nSUMMARY:\n")
            f.write(f"Valid DXF: {results['is_valid_dxf']}\n")
            f.write(f"Entities: {results['entities_count']}\n")
            f.write(f"Errors: {len(results['errors'])}\n")
            f.write(f"Warnings: {len(results['warnings'])}\n")
        
        print(f"\nDetailed report saved to: {args.output}")
    
    # Exit with error code if there are errors
    if results['errors']:
        sys.exit(1)
    else:
        sys.exit(0)


if __name__ == '__main__':
    main()
