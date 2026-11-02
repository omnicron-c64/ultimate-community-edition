#
# Host build of the reply table of the WiFi command channel, and of
# wifi_cmd.cc against stubs of the UART and the RTOS.
#
#   make          build
#   make test     build and run
#   make legacy   build and run the model of the table before the fix; it fails
#
PRJ     = wifireply
RESULT  = result
OUTPUT  = output

PATH_SW = ../../../../software

VPATH   = $(PATH_SW)/io/wifi $(PATH_SW)/test/wifi_reply
STUBS   = $(PATH_SW)/test/wifi_reply/stubs

CC      = gcc
COPTIONS = -std=c99 -g -O0 -Wall -Wextra -Wno-unused-parameter \
           $(addprefix -I, $(VPATH))

CXX     = g++
CXXOPTIONS = -std=c++11 -g -O0 -w -DRISCV=1 -DU64=2 -I$(STUBS) $(addprefix -I, $(VPATH)) \
             -I$(PATH_SW)/io/uart -I$(PATH_SW)/u64ctrl/main -I$(PATH_SW)/system

SRCS_C  = wifi_reply_test.c
OBJS    = $(addprefix $(OUTPUT)/, $(SRCS_C:%.c=%.o))
CMD_SRC = $(PATH_SW)/io/wifi/wifi_cmd.cc $(PATH_SW)/test/wifi_reply/wifi_cmd_test.cc
CMD_DEP = $(wildcard $(PATH_SW)/io/wifi/*.h $(STUBS)/*.h)

.PHONY: all test legacy clean

all: $(RESULT)/$(PRJ) $(RESULT)/wifi_cmd $(RESULT)/wifi_scan $(RESULT)/wifi_feedback

$(OUTPUT)/%.o: %.c wifi_reply.h
	@mkdir -p $(OUTPUT)
	$(CC) $(COPTIONS) -c -o $@ $<

$(RESULT)/$(PRJ): $(OBJS)
	@mkdir -p $(RESULT)
	$(CC) -o $@ $(OBJS)

$(RESULT)/wifi_cmd: $(CMD_SRC) $(CMD_DEP)
	@mkdir -p $(RESULT)
	$(CXX) $(CXXOPTIONS) -o $@ $(CMD_SRC)

$(RESULT)/wifi_scan: $(PATH_SW)/test/wifi_reply/wifi_scan_test.cc $(PATH_SW)/io/wifi/wifi_scan_result.h
	@mkdir -p $(RESULT)
	$(CXX) -std=c++11 -g -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer -I$(STUBS) -I$(PATH_SW)/io/wifi -I$(PATH_SW)/wifi/raw_u64/main -o $@ $<

$(RESULT)/wifi_feedback: $(PATH_SW)/test/wifi_reply/wifi_feedback_test.cc $(PATH_SW)/io/wifi/wifi_connection_feedback.h
	@mkdir -p $(RESULT)
	$(CXX) -std=c++11 -g -Wall -Wextra -fsanitize=address,undefined -fno-omit-frame-pointer -I$(PATH_SW)/io/wifi -o $@ $<

test: all
	@$(RESULT)/wifi_feedback
	@$(RESULT)/wifi_scan
	@$(RESULT)/$(PRJ)
	@mkdir -p $(OUTPUT)
	@$(RESULT)/wifi_cmd > $(OUTPUT)/wifi_cmd.log || { grep '^FAIL' $(OUTPUT)/wifi_cmd.log; tail -n 1 $(OUTPUT)/wifi_cmd.log; exit 1; }
	@tail -n 1 $(OUTPUT)/wifi_cmd.log

legacy:
	@mkdir -p $(OUTPUT)
	$(CC) $(COPTIONS) -DWIFI_REPLY_LEGACY -o $(OUTPUT)/legacy $(PATH_SW)/test/wifi_reply/wifi_reply_test.c
	@$(OUTPUT)/legacy

clean:
	@rm -rf $(OUTPUT) $(RESULT)
