#!/bin/bash

echo "=== Testing Step 4: \$? Variable Expansion ==="
echo

echo "1. Testing basic \$? expansion:"
echo "false"
echo "echo 'Exit status:' \$?"
echo "true" 
echo "echo 'Exit status:' \$?"
echo

echo "2. Testing \$? with builtin commands:"
echo "cd /nonexistent"
echo "echo 'After failed cd:' \$?"
echo "pwd"
echo "echo 'After successful pwd:' \$?"
echo

echo "3. Testing multiple \$? usage in same command:"
echo "false"
echo "echo 'Status:' \$? 'and again:' \$?"
echo

echo "4. Testing \$? with external commands:"
echo "ls nonexistent_file"
echo "echo 'After failed ls:' \$?"
echo "ls"
echo "echo 'After successful ls:' \$?"
echo

echo "5. Testing \$? with exit command:"
echo "exit 42"
echo "echo 'After exit 42:' \$?"
echo

echo "exit"
