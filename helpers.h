#define MAX_CLIENTS 30
#define MAX_LEN 1560
#define STDIN 0
#define IP_LEN 16
#define MAX_TYPE 20
#define OTHER 51
#define UDP_BUFFER_LEN 1551
#define CONTENT_LEN 1500
#define ID_LEN 11
#define TOPIC_LEN 50

#define DIE(condition, message) \
	do { \
		if ((condition)) { \
			fprintf(stderr, "[%d]: %s\n", __LINE__, (message)); \
			perror(""); \
			exit(1); \
		} \
	} while (0)
