#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <unistd.h>
#define lwip_close close
#define lwip_connect connect
#define lwip_select select
#define lwip_ioctl ioctl
