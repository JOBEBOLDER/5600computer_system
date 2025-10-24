#!/bin/bash

echo "=== Quick Shell Test ==="

# Test key features
echo "1. Testing pwd (should show uppercase path):"
echo "pwd" | ./shell56

echo -e "\n2. Testing cd (should ignore arguments and go to HOME):"
echo -e "pwd\ncd /tmp\npwd" | ./shell56

echo -e "\n3. Testing \$? variable:"
echo -e "false\necho \$?\ntrue\necho \$?" | ./shell56

echo -e "\n4. Testing pipes:"
echo -e "ls | grep shell" | ./shell56

echo -e "\n5. Testing redirection:"
echo -e "echo 'test' > quick_test.txt\ncat quick_test.txt" | ./shell56

echo -e "\n6. Testing error handling:"
echo -e "invalid_cmd\necho \$?" | ./shell56

# Cleanup
rm -f quick_test.txt

echo -e "\n=== Quick test completed ==="
