//updated lol2
#ifndef ATTACK_DETECTION_H
#define ATTACK_DETECTION_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stddef.h>
#include <mpi.h>

#define MAX_IPS 100
#define MAX_PORTS 1000
#define MAX_LOG_LINES 10000
#define MAX_IP_LENGTH 16
#define CHECKSUM_POLY 0xEDB88320

typedef struct {
    char ip_address[MAX_IP_LENGTH];
    int failed_logins;
    int port_scans;
    int connection_attempts;
    int is_suspicious;
} SuspiciousIP;

typedef struct {
    char log_line[256];
    char source_ip[MAX_IP_LENGTH];
    int port;
    char event_type[50];
    int timestamp;
} LogEntry;

void parse_log_line(const char *line, LogEntry *entry);
int is_suspicious_ip(SuspiciousIP *ip_list, int list_size, const char *ip);
void add_suspicious_ip(SuspiciousIP *ip_list, int *list_size, const char *ip);
void update_ip_stats(SuspiciousIP *ip_list, int list_size, const char *ip, const char *event_type);
unsigned int calculate_checksum(const char *data, int length);
void print_suspicious_ips(SuspiciousIP *ips, int count);

void load_real_dataset(SuspiciousIP *ips, int *ip_count, int process_rank, int process_size);
void scatter_work_counts(int *my_count, int rank, int size);
int reduce_statistics(SuspiciousIP *local_ips, int local_count);
int detect_distributed_attack_allreduce(SuspiciousIP *local_ips, int local_count);
int validate_log_processing(const char *local_data, int local_length);
void cross_check_suspicious_ips(SuspiciousIP *local_ips, int local_count);

#endif
