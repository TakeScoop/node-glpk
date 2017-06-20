
TEST_INPUTS=$(wildcard test_*.cpp)
TESTS=$(patsubst %.cpp,%.testrunner,$(TEST_INPUTS))

test: $(TESTS)
	./$<

$(TESTS): $(TEST_INPUTS)
	g++ -std=c++14 -ggdb -Wall -Wextra -i/usr/local/include/ -L/usr/local/lib/ -o $@ $< -lgtest -lgmock
