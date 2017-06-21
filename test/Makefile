TEST_INPUTS=$(wildcard test_*.cpp)
TESTS=$(patsubst %.cpp,%.testrunner,$(TEST_INPUTS))

test: $(TESTS)
	./$<
	npm run jstests

$(TESTS): $(TEST_INPUTS)
	g++ -std=c++14 -ggdb -Wall -Wextra -isystem /usr/local/include/ -L/usr/local/lib/ -o $@ $< -lgtest -lgmock
