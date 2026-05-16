#!/usr/bin/env python3
import os
import sys
import argparse
from pathlib import Path

def count_lines(file_path):
    stats = {
        'total': 0,
        'blank': 0,
        'comment': 0,
        'code': 0
    }
    
    in_multiline_comment = False
    in_if_zero = False
    if_zero_depth = 0
    
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                stats['total'] += 1
                stripped = line.strip()
                
                if not stripped:
                    stats['blank'] += 1
                    continue
                
                # Track #if 0 ... #endif blocks (disabled code)
                if stripped.startswith('#') and 'if' in stripped:
                    tokens = stripped[1:].split()
                    if len(tokens) >= 2 and tokens[0] == 'if' and tokens[1] == '0':
                        if_zero_depth += 1
                        in_if_zero = True
                        stats['comment'] += 1
                        continue
                if stripped.startswith('#endif'):
                    if if_zero_depth > 0:
                        if_zero_depth -= 1
                        stats['comment'] += 1
                        if if_zero_depth == 0:
                            in_if_zero = False
                        continue
                    elif in_if_zero:
                        stats['comment'] += 1
                        continue
                if in_if_zero or if_zero_depth > 0:
                    stats['comment'] += 1
                    if stripped.startswith('#if'):
                        if_zero_depth += 1
                    continue
                
                if in_multiline_comment:
                    stats['comment'] += 1
                    if '*/' in stripped:
                        in_multiline_comment = False
                        # Check if there's code after */
                        after = stripped[stripped.index('*/') + 2:].strip()
                        if after and not after.startswith('//'):
                            stats['code'] += 1
                    continue
                
                # Handle // comments
                double_slash = stripped.find('//')
                if double_slash >= 0:
                    before = stripped[:double_slash].strip()
                    if before:
                        stats['code'] += 1
                    stats['comment'] += 1
                    continue
                
                # Handle /* */ comments
                start_comment = stripped.find('/*')
                if start_comment >= 0:
                    before = stripped[:start_comment].strip()
                    end_comment = stripped.find('*/', start_comment + 2)
                    if end_comment >= 0:
                        # Single-line /* ... */ comment
                        if before:
                            stats['code'] += 1
                        stats['comment'] += 1
                        after = stripped[end_comment + 2:].strip()
                        if after and not after.startswith('//'):
                            stats['code'] += 1
                    else:
                        # Start of multi-line comment
                        if before:
                            stats['code'] += 1
                        stats['comment'] += 1
                        in_multiline_comment = True
                    continue
                
                stats['code'] += 1
    except Exception as e:
        print(f"Error reading {file_path}: {e}")
        
    return stats

def main():
    parser = argparse.ArgumentParser(description='Count LOC in C++ files.')
    parser.add_argument('path', nargs='?', default='.', help='Directory to scan (default: current)')
    parser.add_argument('--extensions', nargs='+', default=['.cpp', '.h', '.hpp', '.cxx', '.hxx'], 
                        help='File extensions to include')
    parser.add_argument('--exclude', nargs='+', default=['build','build-*','assets','.git','venv','node_modules','_deps','cmake-build'],
                        help='Directories to exclude. Supports * suffix for prefix matching')
    
    args = parser.parse_args()
    root_path = Path(args.path).resolve()
    extensions = set(args.extensions)
    exclude_dirs = set(args.exclude)
    
    overall_stats = {
        'total': 0,
        'blank': 0,
        'comment': 0,
        'code': 0,
        'files': 0
    }
    
    print(f"Scanning {root_path} for {', '.join(extensions)} files...")
    print(f"Excluding directories: {', '.join(exclude_dirs)}")
    
    for root, dirs, files in os.walk(root_path):
        dirs[:] = [d for d in dirs if not any(
            d == e or (e.endswith('*') and d.startswith(e[:-1]))
            for e in exclude_dirs
        )]
        
        for file in files:
            file_path = Path(root) / file
            if file_path.suffix in extensions:
                file_stats = count_lines(file_path)
                overall_stats['files'] += 1
                for key in ['total', 'blank', 'comment', 'code']:
                    overall_stats[key] += file_stats[key]
    
    print("\nResults:")
    print("-" * 30)
    print(f"Files found:    {overall_stats['files']:10d}")
    print(f"Total lines:    {overall_stats['total']:10d}")
    print(f"Blank lines:    {overall_stats['blank']:10d}")
    print(f"Comment lines:  {overall_stats['comment']:10d}")
    print(f"Code lines:     {overall_stats['code']:10d}")
    print("-" * 30)

if __name__ == "__main__":
    main()
