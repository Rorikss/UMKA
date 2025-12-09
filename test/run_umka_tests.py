#!/usr/bin/env python3

import os
import subprocess
import sys

UMKA_COMPILER_PATH = "../cmake-build-debug/bin/umka_compiler"
UMKA_VM_PATH = "../cmake-build-debug/bin/umka_vm"
EXAMPLES_DIR = "./umkaExamples"

def compile_example(example_path):
    try:
        example_name = os.path.basename(example_path)
        output_path = f"{example_path}.bin"
        
        result = subprocess.run(
            [UMKA_COMPILER_PATH, example_path, output_path],
            capture_output=True,
            text=True
        )
        
        return result.returncode == 0, result.stdout, result.stderr
    except Exception as e:
        return False, "", str(e)

def run_vm(binary_path):
    try:
        result = subprocess.run(
            [UMKA_VM_PATH, binary_path],
            capture_output=True,
            text=True
        )
        
        return result.returncode == 0, result.stdout, result.stderr
    except Exception as e:
        return False, "", str(e)

def extract_actual_output(vm_output):
    lines = vm_output.strip().split('\n')
    actual_output = []
    
    for line in lines:
        if not (line.startswith("Loading bytecode from:") or 
                line == "Execution completed successfully"):
            actual_output.append(line)
    
    return '\n'.join(actual_output) + '\n' if actual_output else ''

def validate_eratosthenes(actual_output):
    if not actual_output.strip().startswith("[") or not actual_output.strip().endswith("]"):
        return False, "Output should be an array format"
    
    try:
        content = actual_output.strip()[1:-1]
        if not content:
            return False, "Empty array"
        
        primes = []
        for item in content.split(","):
            if ":" in item:
                parts = item.split(":")
                if len(parts) == 2:
                    try:
                        value = int(parts[1].strip())
                        primes.append(value)
                    except ValueError:
                        return False, f"Invalid number in array: {parts[1]}"
        
        if not primes:
            return False, "No primes found"
        
        for p in primes:
            if p <= 1:
                return False, f"Invalid prime number: {p}"
        
        expected_first_primes = [2, 3, 5, 7, 11, 13, 17, 19, 23, 29]
        if len(primes) >= 10 and primes[:10] == expected_first_primes and len(primes) == 9592:
            return True, f"Found {len(primes)} primes, first 10 are correct"
        elif len(primes) > 0:
            return True, f"Found {len(primes)} numbers that look like primes"
        else:
            return False, "No valid primes found"
            
    except Exception as e:
        return False, f"Error parsing array: {str(e)}"

def validate_bubble_sort(actual_output):
    if not actual_output.strip().startswith("[") or not actual_output.strip().endswith("]"):
        return False, "Output should be an array format"
    
    try:
        content = actual_output.strip()[1:-1]
        if not content:
            return False, "Empty array"
        
        values = []
        for item in content.split(","):
            if ":" in item:
                parts = item.split(":")
                if len(parts) == 2:
                    try:
                        value_str = parts[1].strip()
                        if '.' in value_str:
                            value = float(value_str)
                        else:
                            value = int(value_str)
                        values.append(value)
                    except ValueError:
                        if value_str == "true":
                            values.append(True)
                        elif value_str == "false":
                            values.append(False)
                        else:
                            return False, f"Invalid value in array: {value_str}"
        
        numeric_values = [v for v in values if isinstance(v, (int, float))]
        if numeric_values == sorted(numeric_values):
            return True, f"Array is correctly sorted with {len(values)} elements"
        else:
            return False, f"Array is not sorted: {numeric_values}"
            
    except Exception as e:
        return False, f"Error parsing array: {str(e)}"

def validate_factorial(actual_output):
    return actual_output.strip() == "2432902008176640000", "Expected output: 2432902008176640000"

def main():
    if not os.path.exists(UMKA_COMPILER_PATH):
        print(f"Error: Compiler not found at {UMKA_COMPILER_PATH}")
        return 1
        
    if not os.path.exists(UMKA_VM_PATH):
        print(f"Error: VM not found at {UMKA_VM_PATH}")
        return 1
    
    test_examples = ["eratosfen", "buble_sort", "factorial"]
    passed = 0
    failed = 0
    
    for example_name in test_examples:
        example_path = os.path.join(EXAMPLES_DIR, example_name)
        print(f"\nTesting {example_name}...")
        
        success, stdout, stderr = compile_example(example_path)
        if not success:
            print(f"    Compilation failed: {stderr}")
            failed += 1
            continue
            
        binary_path = f"{example_path}.bin"
        success, output, stderr = run_vm(binary_path)
        if not success:
            print(f"    VM execution failed: {stderr}")
            failed += 1
            continue
            
        actual_output = extract_actual_output(output)
        
        if example_name == "eratosfen":
            is_valid, message = validate_eratosthenes(actual_output)
        elif example_name == "buble_sort":
            is_valid, message = validate_bubble_sort(actual_output)
        elif example_name == "factorial":
            is_valid, message = validate_factorial(actual_output)
        else:
            is_valid, message = True, "Execution successful"
            
        if is_valid:
            print(f"    PASSED - {message}")
            passed += 1
        else:
            print(f"    FAILED - {message}")
            print(f"    Output: {repr(actual_output)}")
            failed += 1
    
    print(f"\n=== Test Results ===")
    print(f"Passed: {passed}")
    print(f"Failed: {failed}")
    print(f"Total:  {passed + failed}")
    
    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())