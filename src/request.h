#ifndef __REQUEST_H__

void request_handle(int fd);
void request_error(int fd, char* cause, char* errnum, char* shortmsg, char* longmsg);
void request_read_headers(int fd);
int  request_parse_uri(char* uri, char* filename, char* cgiargs);

#endif // __REQUEST_H__
