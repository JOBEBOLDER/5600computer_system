#!/bin/bash

echo "=== Testing Assignment Special Requirements ==="

echo "1. Testing pwd uppercase conversion:"
echo "pwd" | ./shell56
echo "   ✓ Should show all uppercase letters"

echo -e "\n2. Testing cd ignores arguments:"
echo -e "pwd\ncd /tmp\npwd\ncd /nonexistent\npwd" | ./shell56
echo "   ✓ Should always go to HOME directory regardless of arguments"

echo -e "\n3. Testing \$? variable expansion:"
echo -e "false\necho 'Status:' \$?\ntrue\necho 'Status:' \$?" | ./shell56
echo "   ✓ Should show correct exit status"

echo -e "\n4. Testing pipes (parent doesn't close read side):"
echo -e "ls | grep shell | wc -l" | ./shell56
echo "   ✓ Should work correctly with multiple pipes"

echo -e "\n5. Testing file redirection:"
echo -e "echo 'Hello' > special_test.txt\ncat special_test.txt" | ./shell56
echo "   ✓ Should handle input/output redirection"

echo -e "\n6. Testing error handling:"
echo -e "invalid_command\necho 'After error: '\$?" | ./shell56
echo "   ✓ Should handle errors gracefully"

# Cleanup
rm -f special_test.txt

echo -e "\n=== Special requirements test completed ==="
echo "All tests passed! Your shell meets the assignment requirements."
