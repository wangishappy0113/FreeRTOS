#!/usr/bin/env python3
import json
import os
import sys
from pathlib import Path

def fix_compile_commands(input_file, output_file):
    """
    Fix compile_commands.json for better Joern compatibility
    """
    with open(input_file, 'r') as f:
        compile_commands = json.load(f)
    
    current_dir = Path.cwd().resolve()
    fixed_commands = []
    
    for entry in compile_commands:
        # Get the directory and file paths
        directory = Path(entry['directory']).resolve()
        
        # Handle relative file paths
        if entry['file'].startswith('/'):
            # Absolute path
            file_path = Path(entry['file']).resolve()
        else:
            # Relative path - resolve relative to directory
            file_path = (directory / entry['file']).resolve()
        
        # Fix the arguments
        args = entry['arguments']
        fixed_args = []
        
        for arg in args:
            if arg == 'arm-none-eabi-gcc':
                # Replace with standard gcc for Joern
                fixed_args.append('gcc')
            elif arg == 'arm-none-eabi-g++':
                fixed_args.append('g++')
            elif arg.startswith('-mcpu=') or arg.startswith('-mthumb'):
                # Skip ARM-specific flags that might confuse Joern
                continue
            else:
                fixed_args.append(arg)
        
        # Create the fixed entry
        fixed_entry = {
            'directory': str(directory),
            'file': str(file_path),
            'arguments': fixed_args
        }
        
        # Verify the file exists
        if file_path.exists():
            fixed_commands.append(fixed_entry)
        else:
            print(f"Warning: File not found: {file_path}")
    
    # Write the fixed compile commands
    with open(output_file, 'w') as f:
        json.dump(fixed_commands, f, indent=2)
    
    print(f"Fixed {len(fixed_commands)} compilation entries")
    print(f"Output written to: {output_file}")

if __name__ == '__main__':
    input_file = 'compile_commands.json'
    output_file = 'compile_commands_fixed.json'
    
    if not os.path.exists(input_file):
        print(f"Error: {input_file} not found")
        sys.exit(1)
    
    fix_compile_commands(input_file, output_file)
