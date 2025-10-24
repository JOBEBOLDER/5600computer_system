#!/bin/bash

echo "=== CS 5600 Lab 2 Shell Test Suite ==="
echo "Testing shell56 implementation..."
echo

# Test 1: Basic builtin commands
echo "Test 1: Basic builtin commands"
echo "pwd"
echo "cd /tmp"
echo "pwd"
echo "exit"
echo "---"
echo "pwd" | ./shell56
echo "cd /tmp" | ./shell56
echo "pwd" | ./shell56
echo

# Test 2: $? variable expansion
echo "Test 2: \$? variable expansion"
echo "false"
echo "echo 'After false: '\$?"
echo "true"
echo "echo 'After true: '\$?"
echo "exit"
echo "---"
echo -e "false\necho 'After false: '\$?\ntrue\necho 'After true: '\$?\nexit" | ./shell56
echo

# Test 3: External commands
echo "Test 3: External commands"
echo "ls"
echo "echo 'Hello World'"
echo "exit"
echo "---"
echo -e "ls\necho 'Hello World'\nexit" | ./shell56
echo

# Test 4: File redirection
echo "Test 4: File redirection"
echo "echo 'Test content' > test_output.txt"
echo "cat test_output.txt"
echo "echo 'Input test' > input.txt"
echo "cat < input.txt"
echo "exit"
echo "---"
echo -e "echo 'Test content' > test_output.txt\ncat test_output.txt\necho 'Input test' > input.txt\ncat < input.txt\nexit" | ./shell56
echo

# Test 5: Pipes
echo "Test 5: Pipes"
echo "ls | grep shell"
echo "ls | wc -l"
echo "echo 'hello world' | tr 'a-z' 'A-Z'"
echo "exit"
echo "---"
echo -e "ls | grep shell\nls | wc -l\necho 'hello world' | tr 'a-z' 'A-Z'\nexit" | ./shell56
echo

# Test 6: Error handling
echo "Test 6: Error handling"
echo "cd /nonexistent"
echo "echo 'After failed cd: '\$?"
echo "invalid_command"
echo "echo 'After invalid command: '\$?"
echo "exit"
echo "---"
echo -e "cd /nonexistent\necho 'After failed cd: '\$?\ninvalid_command\necho 'After invalid command: '\$?\nexit" | ./shell56
echo

# Test 7: Empty commands and edge cases
echo "Test 7: Empty commands and edge cases"
echo ""
echo "echo 'Empty line test'"
echo "exit"
echo "---"
echo -e "\necho 'Empty line test'\nexit" | ./shell56
echo

# Test 8: Multiple $? usage
echo "Test 8: Multiple \$? usage in same command"
echo "false"
echo "echo 'Status:' \$? 'and again:' \$?"
echo "true"
echo "echo 'Status:' \$? 'and again:' \$?"
echo "exit"
echo "---"
echo -e "false\necho 'Status:' \$? 'and again:' \$?\ntrue\necho 'Status:' \$? 'and again:' \$?\nexit" | ./shell56
echo

# Test 9: Complex pipeline
echo "Test 9: Complex pipeline"
echo "ls | grep shell | wc -l"
echo "echo 'Complex pipeline test completed'"
echo "exit"
echo "---"
echo -e "ls | grep shell | wc -l\necho 'Complex pipeline test completed'\nexit" | ./shell56
echo

# Test 10: Signal handling (basic test)
echo "Test 10: Signal handling (basic test)"
echo "echo 'Signal handling test'"
echo "echo 'Shell should not exit on ^C in interactive mode'"
echo "exit"
echo "---"
echo -e "echo 'Signal handling test'\necho 'Shell should not exit on ^C in interactive mode'\nexit" | ./shell56
echo

echo "=== All tests completed ==="
echo "Cleaning up test files..."
rm -f test_output.txt input.txt
echo "Test files cleaned up."
