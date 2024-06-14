CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=c11 -fPIC -Iinclude

SRC_DIR = src
INCLUDE_DIR = include
TEST_DIR = test
LOG_DIR = log
LOG_FILE = $(LOG_DIR)/logfile.log
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:$(SRC_DIR)/%.c=$(SRC_DIR)/%.o)

STATIC_LIB = libmy_secmalloc.a
DYNAMIC_LIB = libmy_secmalloc.so
TEST_EXEC = test_exec

CRITERION_FLAGS = -lcriterion

all: static dynamic $(TEST_EXEC)
	mkdir $(LOG_DIR)
	touch $(LOG_FILE)
	export MSM_OUTPUT=$(LOG_FILE)
static: $(STATIC_LIB)

dynamic: $(DYNAMIC_LIB)

test_static: static $(TEST_EXEC)
	touch $(LOG_FILE)
	env MSM_OUTPUT=$(PWD)/$(LOG_FILE) ./$(TEST_EXEC)

test_dynamic: dynamic $(TEST_EXEC)
	touch $(LOG_FILE)
	env MSM_OUTPUT=$(PWD)/$(LOG_FILE) \
	LD_PRELOAD=$(PWD)/$(DYNAMIC_LIB) \
	LD_LIBRARY_PATH=$(PWD):$(LD_LIBRARY_PATH) \
	./$(TEST_EXEC)

clean:
	rm -f $(OBJ) $(STATIC_LIB) $(DYNAMIC_LIB) $(TEST_EXEC)
	rm -f $(LOG_FILE)
	rm -r $(LOG_DIR)
$(SRC_DIR)/%.o: $(SRC_DIR)/%.c $(INCLUDE_DIR)/my_secmalloc.h $(INCLUDE_DIR)/my_secmalloc.private.h
	$(CC) $(CFLAGS) -c $< -o $@

$(STATIC_LIB): $(OBJ)
	ar rcs $@ $^

$(DYNAMIC_LIB): $(OBJ)
	$(CC) -shared -o $@ $^

$(TEST_EXEC): $(TEST_DIR)/test.c $(STATIC_LIB)
	$(CC) $(CFLAGS) -o $@ $< $(STATIC_LIB) $(CRITERION_FLAGS)

.PHONY: all static dynamic clean test_static test_dynamic