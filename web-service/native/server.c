#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include "gx.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define MAX_BODY (100u * 1024 * 1024)
#define MAX_HEAD 16384
static const char *root = "/data", *www = "/PROGRAM/revival-web/www",
                  *state = "/run/revival-web",
                  *keyfile = "/etc/revival-web/key";
static char key[65], printer_host[64] = "127.0.0.1", bind_host[64] = "0.0.0.0";
static int printer_port = 8899, camera_port = 8080, port = 8081, rootfd = -1,
           stagefd = -1;
static volatile sig_atomic_t children;
static char cleanup_output[80];
static void worker_exit(int number) {
  if (*cleanup_output && stagefd >= 0)
    unlinkat(stagefd, cleanup_output, 0);
  _exit(128 + number);
}
static int sendall(int fd, const void *p, size_t n) {
  while (n) {
    ssize_t s = send(fd, p, n, MSG_NOSIGNAL);
    if (s <= 0)
      return -1;
    p = (const char *)p + s;
    n -= s;
  }
  return 0;
}
static void response(int fd, int status, const char *mime, const void *body,
                     size_t n) {
  char h[768];
  int l =
      snprintf(h, sizeof(h),
               "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: "
               "%zu\r\nConnection: "
               "close\r\nCache-Control: no-store\r\nX-Content-Type-Options: "
               "nosniff\r\nReferrer-Policy: "
               "no-referrer\r\nContent-Security-Policy: default-src 'self'; "
               "img-src 'self' blob:; "
               "style-src 'self'; script-src 'self'; frame-ancestors 'none'; "
               "base-uri 'none'\r\n\r\n",
               status, status < 300 ? "OK" : "Error", mime, n);
  sendall(fd, h, l);
  sendall(fd, body, n);
}
static void json(int fd, int code, const char *text) {
  response(fd, code, "application/json", text, strlen(text));
}
static void error(int fd, int code, const char *text) {
  char s[512];
  snprintf(s, sizeof(s), "{\"error\":\"%s\"}", text);
  json(fd, code, s);
}
static void quote(FILE *f, const char *s) {
  fputc('"', f);
  for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
    if (*p == '"' || *p == '\\')
      fprintf(f, "\\%c", *p);
    else if (*p < 32)
      fprintf(f, "\\u%04x", *p);
    else
      fputc(*p, f);
  }
  fputc('"', f);
}
static int extension(const char *s, const char *ext) {
  size_t a = strlen(s), b = strlen(ext);
  return a > b && !strcasecmp(s + a - b, ext);
}
static int utf8(const unsigned char *s) {
  while (*s) {
    unsigned c = *s++;
    if (c < 128)
      continue;
    unsigned n, min;
    if (c >= 0xc2 && c <= 0xdf) {
      n = 1;
      c &= 31;
      min = 128;
    } else if (c >= 0xe0 && c <= 0xef) {
      n = 2;
      c &= 15;
      min = 2048;
    } else if (c >= 0xf0 && c <= 0xf4) {
      n = 3;
      c &= 7;
      min = 65536;
    } else
      return 0;
    while (n--) {
      if ((*s & 0xc0) != 0x80)
        return 0;
      c = (c << 6) | (*s++ & 63);
    }
    if (c < min || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
      return 0;
  }
  return 1;
}
static int filename(const char *s) {
  size_t n = strlen(s);
  if (!n || n > 180 || s[0] == '.' || strchr(s, '/') || strchr(s, '\\') ||
      !utf8((const unsigned char *)s))
    return 0;
  for (size_t i = 0; i < n; i++)
    if ((unsigned char)s[i] < 32 || (unsigned char)s[i] == 127)
      return 0;
  return extension(s, ".gx") || extension(s, ".gcode");
}
static int unescape(const char *src, char *out, size_t cap) {
  size_t n = 0;
  for (; *src; src++) {
    unsigned c = (unsigned char)*src;
    if (c == '%') {
      char a = src[1], b = src[1] ? src[2] : 0;
      if (!a || !b || !isxdigit((unsigned char)a) ||
          !isxdigit((unsigned char)b))
        return -1;
      char v[3] = {a, b, 0};
      c = strtoul(v, NULL, 16);
      if (c == 0)
        return -1;
      src += 2;
    }
    if (n + 1 >= cap)
      return -1;
    out[n++] = (char)c;
  }
  out[n] = 0;
  return 0;
}
static int scratch(void) {
  char p[1024];
  snprintf(p, sizeof(p), "%s/.revival-web/input-XXXXXX", root);
  int fd = mkstemp(p);
  if (fd >= 0) {
    fchmod(fd, 0600);
    unlink(p);
  }
  return fd;
}
static int lockfile(const char *name, int nonblock) {
  char p[1024];
  snprintf(p, sizeof(p), "%s/%s", state, name);
  int fd = open(p, O_CREAT | O_RDWR | O_CLOEXEC, 0600);
  if (fd < 0)
    return -1;
  if (flock(fd, LOCK_EX | (nonblock ? LOCK_NB : 0))) {
    close(fd);
    return -1;
  }
  return fd;
}
static int printer_command(const char *command, char *reply, size_t cap) {
  int fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd < 0)
    return -1;
  struct timeval tv = {3, 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  struct sockaddr_in a = {.sin_family = AF_INET,
                          .sin_port = htons(printer_port)};
  if (inet_pton(AF_INET, printer_host, &a.sin_addr) != 1 ||
      connect(fd, (struct sockaddr *)&a, sizeof(a))) {
    close(fd);
    return -1;
  }
  if (sendall(fd, command, strlen(command))) {
    close(fd);
    return -1;
  }
  size_t n = 0;
  reply[0] = 0;
  while (n + 1 < cap) {
    ssize_t r = recv(fd, reply + n, cap - n - 1, 0);
    if (r <= 0) {
      close(fd);
      return -1;
    }
    n += r;
    reply[n] = 0;
    size_t end = n;
    while (end && (reply[end - 1] == '\r' || reply[end - 1] == '\n' ||
                   reply[end - 1] == ' '))
      end--;
    if (end >= 2 && reply[end - 2] == 'o' && reply[end - 1] == 'k' &&
        (end == 2 || reply[end - 3] == '\n' || reply[end - 3] == '\r')) {
      close(fd);
      return 0;
    }
  }
  close(fd);
  return -1;
}
static int ready(char *reply, size_t cap) {
  int l = lockfile("printer.lock", 0);
  if (l < 0)
    return 0;
  int r = printer_command("~M119\r\n", reply, cap);
  close(l);
  return r == 0 && strstr(reply, "MachineStatus: READY") &&
         strstr(reply, "MoveMode: READY");
}
static void status(int fd) {
  char r[4096] = "", t[4096] = "", progress[4096] = "";
  int l = lockfile("printer.lock", 1);
  if (l < 0) {
    error(fd, 503, "Printer query in progress; try again");
    return;
  }
  int ok = printer_command("~M119\r\n", r, sizeof(r)) == 0;
  if (ok) {
    printer_command("~M105\r\n", t, sizeof(t));
    printer_command("~M27\r\n", progress, sizeof(progress));
  }
  close(l);
  int can_change =
      ok && strstr(r, "MachineStatus: READY") && strstr(r, "MoveMode: READY");
  char *b = NULL;
  size_t n = 0;
  FILE *f = open_memstream(&b, &n);
  fprintf(f, "{\"connected\":%s,\"ready\":%s,\"state\":", ok ? "true" : "false",
          can_change ? "true" : "false");
  const char *s = !ok                     ? "Unavailable"
                  : can_change            ? "Ready"
                  : strstr(r, "PAUSED")   ? "Paused"
                  : strstr(r, "BUILDING") ? "Printing"
                                          : "Busy";
  quote(f, s);
  fprintf(f, ",\"raw\":");
  quote(f, r);
  fprintf(f, ",\"temperature\":");
  quote(f, t);
  unsigned long long done = 0, total = 0;
  const char *at = strstr(progress, "SD printing byte ");
  fprintf(f, ",\"progress\":");
  if (at && sscanf(at, "SD printing byte %llu/%llu", &done, &total) == 2 &&
      total && done <= total)
    fprintf(f, "{\"current\":%llu,\"total\":%llu,\"percent\":%.1f}", done,
            total, (double)done / total * 100.0);
  else
    fprintf(f, "null");
  fprintf(f, "}");
  fclose(f);
  response(fd, 200, "application/json", b, n);
  free(b);
}
static void reply_field(FILE *f, const char *reply, const char *label) {
  char value[256] = "";
  const char *p = strstr(reply, label);
  if (p && (p == reply || p[-1] == '\n')) {
    p += strlen(label);
    while (*p == ' ')
      p++;
    size_t n = strcspn(p, "\r\n");
    if (n >= sizeof(value))
      n = sizeof(value) - 1;
    memcpy(value, p, n);
  }
  quote(f, value);
}
static void printer_info(int fd) {
  char reply[4096] = "";
  int l = lockfile("printer.lock", 0);
  int result = l < 0 ? -1 : printer_command("~M115\r\n", reply, sizeof(reply));
  if (l >= 0)
    close(l);
  if (result) {
    error(fd, 503, "Printer information unavailable; try again");
    return;
  }
  char *b = NULL;
  size_t n = 0;
  FILE *f = open_memstream(&b, &n);
  fprintf(f, "{\"machineName\":");
  reply_field(f, reply, "Machine Name:");
  fprintf(f, ",\"machineType\":");
  reply_field(f, reply, "Machine Type:");
  fprintf(f, ",\"firmware\":");
  reply_field(f, reply, "Firmware:");
  fprintf(f, ",\"serialNumber\":");
  reply_field(f, reply, "SN:");
  fprintf(f, "}");
  fclose(f);
  response(fd, 200, "application/json", b, n);
  free(b);
}
/* The unmodified stock camera owns port 8080. Proxy only its fixed loopback
 * URLs. */
static void camera(int fd, int stream) {
  int lock =
      lockfile(stream ? "camera-stream.lock" : "camera-snapshot.lock", 1);
  if (lock < 0) {
    error(fd, 409, "Camera connection already in use; try a snapshot");
    return;
  }
  int upstream = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  struct timeval tv = {5, 0};
  setsockopt(upstream, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(upstream, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  struct sockaddr_in addr = {.sin_family = AF_INET,
                             .sin_port = htons(camera_port)};
  inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
  char request[256], headers[8193];
  size_t n = 0;
  snprintf(request, sizeof(request),
           "GET /?action=%s HTTP/1.0\r\nHost: 127.0.0.1:%d\r\nConnection: "
           "close\r\n\r\n",
           stream ? "stream" : "snapshot", camera_port);
  if (upstream < 0 ||
      connect(upstream, (struct sockaddr *)&addr, sizeof(addr)) ||
      sendall(upstream, request, strlen(request)))
    goto unavailable;
  while (n < sizeof(headers) - 1) {
    if (recv(upstream, headers + n, 1, 0) != 1)
      goto unavailable;
    n++;
    if (n >= 4 && !memcmp(headers + n - 4, "\r\n\r\n", 4))
      break;
  }
  if (n >= sizeof(headers) - 1)
    goto unavailable;
  headers[n] = 0;
  int status_code = 0;
  if (sscanf(headers, "HTTP/%*s %d", &status_code) != 1 || status_code != 200)
    goto unavailable;
  char mime[256] = "";
  unsigned long length = 0;
  char *line, *save;
  for (line = strtok_r(headers, "\r\n", &save); line;
       line = strtok_r(NULL, "\r\n", &save)) {
    if (!strncasecmp(line, "Content-Type:", 13)) {
      const char *p = line + 13;
      while (*p == ' ')
        p++;
      if (strlen(p) >= sizeof(mime))
        goto unavailable;
      strcpy(mime, p);
    }
    if (!strncasecmp(line, "Content-Length:", 15))
      length = strtoul(line + 15, NULL, 10);
    if (!strncasecmp(line, "Transfer-Encoding:", 18))
      goto unavailable;
  }
  if (stream) {
    if (strncasecmp(mime, "multipart/x-mixed-replace;", 26) ||
        !strstr(mime, "boundary="))
      goto unavailable;
    for (const char *p = mime; *p; p++)
      if ((unsigned char)*p < 32 || (unsigned char)*p > 126)
        goto unavailable;
    char head[512];
    int count = snprintf(head, sizeof(head),
                         "HTTP/1.1 200 OK\r\nContent-Type: "
                         "%s\r\nCache-Control: no-store\r\nConnection: "
                         "close\r\nX-Content-Type-Options: nosniff\r\n\r\n",
                         mime);
    if (sendall(fd, head, count))
      goto done;
    char bytes[8192];
    ssize_t got;
    while ((got = recv(upstream, bytes, sizeof(bytes), 0)) > 0)
      if (sendall(fd, bytes, got))
        break;
  } else {
    if (strcasecmp(mime, "image/jpeg") || (length && length < 4) ||
        length > 2 * 1024 * 1024)
      goto unavailable;
    size_t cap = length ? length : 2 * 1024 * 1024;
    unsigned char *data = malloc(cap);
    if (!data)
      goto unavailable;
    size_t at = 0;
    ssize_t got = 0;
    while (at < cap) {
      got = recv(upstream, data + at, cap - at, 0);
      if (got <= 0)
        break;
      at += got;
    }
    if (at >= 4 && ((length && at == length) || (!length && got == 0)) &&
        data[0] == 0xff && data[1] == 0xd8 && data[at - 2] == 0xff &&
        data[at - 1] == 0xd9)
      response(fd, 200, "image/jpeg", data, at);
    else
      error(fd, 503, "Camera frame unavailable; retry the camera");
    free(data);
  }
  goto done;
unavailable:
  error(fd, 503, "Camera unavailable; connect a USB camera and retry");
done:
  if (upstream >= 0)
    close(upstream);
  close(lock);
}
static void identity(char *p, size_t cap, const struct stat *s) {
  snprintf(p, cap, "%llu-%llu-%lld-%ld", (unsigned long long)s->st_ino,
           (unsigned long long)s->st_size, (long long)s->st_mtime,
           s->st_mtim.tv_nsec);
}
static int notification_read(char *name, size_t cap, char *etag, size_t idcap) {
  char path[1024];
  snprintf(path, sizeof(path), "%s/upload-notification", state);
  FILE *f = fopen(path, "r");
  if (!f)
    return 0;
  int ok = fgets(name, cap, f) != NULL && fgets(etag, idcap, f) != NULL;
  fclose(f);
  if (!ok)
    return 0;
  name[strcspn(name, "\r\n")] = 0;
  etag[strcspn(etag, "\r\n")] = 0;
  struct stat s;
  char current[128];
  if (!filename(name) || fstatat(rootfd, name, &s, AT_SYMLINK_NOFOLLOW) ||
      !S_ISREG(s.st_mode))
    return 0;
  identity(current, sizeof(current), &s);
  return !strcmp(current, etag);
}
static void notify_upload(const char *name) {
  struct stat s;
  if (fstatat(rootfd, name, &s, AT_SYMLINK_NOFOLLOW))
    return;
  char etag[128], path[1024], temp[1024];
  identity(etag, sizeof(etag), &s);
  snprintf(path, sizeof(path), "%s/upload-notification", state);
  snprintf(temp, sizeof(temp), "%s/upload-notification.new", state);
  FILE *f = fopen(temp, "w");
  if (!f)
    return;
  fprintf(f, "%s\n%s\n", name, etag);
  int failed = fclose(f);
  if (!failed)
    rename(temp, path);
  else
    unlink(temp);
}
static void notifications(int fd) {
  char name[256], etag[128];
  if (!notification_read(name, sizeof(name), etag, sizeof(etag))) {
    json(fd, 200, "null");
    return;
  }
  char *b = NULL;
  size_t n = 0;
  FILE *f = open_memstream(&b, &n);
  fprintf(f, "{\"type\":\"uploadComplete\",\"name\":");
  quote(f, name);
  fprintf(f, ",\"etag\":");
  quote(f, etag);
  fprintf(f, "}");
  fclose(f);
  response(fd, 200, "application/json", b, n);
  free(b);
}
static void dismiss_notification(int fd, const char *expected) {
  int lock = lockfile("mutation.lock", 1);
  if (lock < 0) {
    error(fd, 409, "Another file operation is in progress");
    return;
  }
  char name[256], etag[128], path[1024];
  if (!*expected ||
      !notification_read(name, sizeof(name), etag, sizeof(etag)) ||
      strcmp(expected, etag)) {
    close(lock);
    error(fd, 409, "Upload notification changed; refresh before dismissing");
    return;
  }
  snprintf(path, sizeof(path), "%s/upload-notification", state);
  unlink(path);
  close(lock);
  response(fd, 204, "application/json", "", 0);
}
static void file_json(FILE *f, const char *name, const struct stat *s) {
  char id[128];
  identity(id, sizeof(id), s);
  fprintf(f, "{\"name\":");
  quote(f, name);
  fprintf(f, ",\"path\":");
  quote(f, name);
  fprintf(f,
          ",\"origin\":\"local\",\"type\":\"machinecode\",\"typePath\":["
          "\"machinecode\","
          "\"gcode\"],\"size\":%llu,\"date\":%lld,\"etag\":",
          (unsigned long long)s->st_size, (long long)s->st_mtime);
  quote(f, id);
  int in = openat(rootfd, name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  struct gx_meta m = {0};
  uint32_t b, c;
  int valid = in >= 0 && !gx_header(in, &m, &b, &c);
  if (in >= 0)
    close(in);
  fprintf(f,
          ",\"gx\":%s,\"thumbnail\":%s,\"gcodeAnalysis\":{"
          "\"estimatedPrintTime\":%u,\"filament\":"
          "{\"length\":%u}},\"layerHeight\":%.3f,\"nozzle\":%u}",
          valid ? "true" : "false", valid && c > b ? "true" : "false",
          m.seconds, m.filament, m.layer / 1000.0, m.nozzle);
}
static void files(int fd) {
  DIR *d = fdopendir(openat(rootfd, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC));
  if (!d) {
    error(fd, 500, "Cannot read printer storage");
    return;
  }
  char *b = NULL;
  size_t n = 0;
  FILE *f = open_memstream(&b, &n);
  fprintf(f, "{\"files\":[");
  struct dirent *e;
  int count = 0;
  while ((e = readdir(d))) {
    struct stat s;
    if (!filename(e->d_name) ||
        fstatat(rootfd, e->d_name, &s, AT_SYMLINK_NOFOLLOW) ||
        !S_ISREG(s.st_mode))
      continue;
    if (count++)
      fputc(',', f);
    file_json(f, e->d_name, &s);
    if (count >= 2048)
      break;
  }
  closedir(d);
  struct statvfs v = {0};
  fstatvfs(rootfd, &v);
  fprintf(f, "],\"free\":%llu,\"total\":%llu}",
          (unsigned long long)v.f_bavail * v.f_frsize,
          (unsigned long long)v.f_blocks * v.f_frsize);
  fclose(f);
  response(fd, 200, "application/json", b, n);
  free(b);
}
static void preview(int fd, const char *name) {
  int in = openat(rootfd, name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  struct gx_meta m;
  uint32_t a, b;
  unsigned char magic[2];
  if (in < 0 || gx_header(in, &m, &a, &b) || b <= a || b - a > 1024 * 1024 ||
      pread(in, magic, 2, a) != 2 || memcmp(magic, "BM", 2)) {
    if (in >= 0)
      close(in);
    error(fd, 404, "No preview available");
    return;
  }
  size_t n = b - a;
  unsigned char *p = malloc(n);
  if (!p || pread(in, p, n, a) != (ssize_t)n) {
    close(in);
    free(p);
    error(fd, 500, "Cannot read preview");
    return;
  }
  close(in);
  response(fd, 200, "image/bmp", p, n);
  free(p);
}
struct request {
  char method[12], path[1024], host[256], origin[512], type[256], api[128],
      etag[128], expect[80];
  size_t length;
};
static int equalkey(const char *p) {
  if (strlen(p) != 64)
    return 0;
  unsigned diff = 0;
  for (int i = 0; i < 64; i++)
    diff |= (unsigned char)p[i] ^ (unsigned char)key[i];
  return diff == 0;
}
static int origin_ok(struct request *r) {
  if (!*r->origin)
    return 1;
  char origin[512];
  snprintf(origin, sizeof(origin), "http://%s", r->host);
  return !strcmp(origin, r->origin);
}
static int get_headers(int fd, struct request *r) {
  char h[MAX_HEAD + 1];
  size_t n = 0;
  while (n < MAX_HEAD) {
    ssize_t k = recv(fd, h + n, 1, 0);
    if (k != 1)
      return -1;
    n++;
    if (n >= 4 && !memcmp(h + n - 4, "\r\n\r\n", 4))
      break;
  }
  if (n == MAX_HEAD)
    return -1;
  h[n] = 0;
  char *save, *line = strtok_r(h, "\r\n", &save);
  char version[16];
  if (!line ||
      sscanf(line, "%11s %1023s %15s", r->method, r->path, version) != 3 ||
      strcmp(version, "HTTP/1.1"))
    return -1;
  int length_seen = 0;
  while ((line = strtok_r(NULL, "\r\n", &save))) {
    char *p = strchr(line, ':');
    if (!p)
      return -1;
    *p++ = 0;
    while (*p == ' ' || *p == '\t')
      p++;
#define HEADER(a, b)                                                           \
  if (!strcasecmp(line, a)) {                                                  \
    if (strlen(p) >= sizeof(r->b))                                             \
      return -1;                                                               \
    strcpy(r->b, p);                                                           \
    continue;                                                                  \
  }
    HEADER("Host", host)
    HEADER("Origin", origin)
    HEADER("Content-Type", type)
    HEADER("X-Api-Key", api)
    HEADER("If-Match", etag)
    HEADER("Expect", expect)
#undef HEADER
    if (!strcasecmp(line, "Content-Length")) {
      if (length_seen++)
        return -1;
      char *end;
      errno = 0;
      unsigned long long l = strtoull(p, &end, 10);
      if (errno || *end || p == end || l > MAX_BODY + 65536)
        return -1;
      r->length = l;
    }
    if (!strcasecmp(line, "Transfer-Encoding"))
      return -1;
  }
  if (!*r->host || strchr(r->path, '#'))
    return -1;
  char host[256];
  snprintf(host, sizeof(host), "%s", r->host);
  char *colon = strchr(host, ':');
  if (colon)
    *colon = 0;
  struct in_addr address;
  if (strcmp(host, "localhost") && inet_pton(AF_INET, host, &address) != 1)
    return -1;
  char *q = strchr(r->path, '?');
  if (q)
    *q = 0;
  return 0;
}
struct upload {
  char name[256];
  size_t offset, length;
  int print;
};
static int multipart(const unsigned char *p, size_t n, const char *type,
                     struct upload *u) {
  const char *b = strstr(type, "boundary=");
  if (!b)
    return -1;
  b += 9;
  char boundary[100];
  size_t l = strcspn(b, "; \r\n");
  if (l > 2 && b[0] == '"' && b[l - 1] == '"') {
    b++;
    l -= 2;
  }
  if (l < 1 || l > 70)
    return -1;
  snprintf(boundary, sizeof(boundary), "--%.*s", (int)l, b);
  size_t bl = l + 2, at = 0;
  int found = 0;
  for (int parts = 0; parts < 16; parts++) {
    if (at + bl + 2 > n || memcmp(p + at, boundary, bl))
      return -1;
    at += bl;
    if (!memcmp(p + at, "--", 2))
      return found ? 0 : -1;
    if (memcmp(p + at, "\r\n", 2))
      return -1;
    at += 2;
    const unsigned char *end = memmem(p + at, n - at, "\r\n\r\n", 4);
    if (!end || end - (p + at) > 8192)
      return -1;
    size_t hn = end - (p + at);
    char head[8193];
    memcpy(head, p + at, hn);
    head[hn] = 0;
    at = (end - p) + 4;
    char delimiter[104];
    snprintf(delimiter, sizeof(delimiter), "\r\n%s", boundary);
    const unsigned char *next = memmem(p + at, n - at, delimiter, bl + 2);
    if (!next)
      return -1;
    size_t size = next - (p + at);
    if (strstr(head, "name=\"file\"")) {
      if (found++)
        return -1;
      char *name = strstr(head, "filename=\"");
      if (!name)
        return -1;
      name += 10;
      char *stop = strchr(name, '"');
      if (!stop || stop - name >= 180)
        return -1;
      memcpy(u->name, name, stop - name);
      u->name[stop - name] = 0;
      if (!filename(u->name) || size < 1 || size > MAX_BODY)
        return -1;
      u->offset = at;
      u->length = size;
    } else if (strstr(head, "name=\"print\"")) {
      u->print = size == 4 && !memcmp(p + at, "true", 4);
    }
    at = (next - p) + 2;
  }
  return -1;
}
static int start_print(const char *name) {
  char reply[4096], cmd[512];
  int l = lockfile("printer.lock", 0);
  if (l < 0)
    return -1;
  int r = printer_command("~M119\r\n", reply, sizeof(reply));
  if (!r && (!strstr(reply, "MachineStatus: READY") ||
             !strstr(reply, "MoveMode: READY")))
    r = -1;
  if (!r)
    r = printer_command("~M601 S1\r\n", reply, sizeof(reply));
  if (!r && !strstr(reply, "Control Success"))
    r = -1;
  if (!r) {
    snprintf(cmd, sizeof(cmd), "~M23 0:/user/%s\r\n", name);
    r = printer_command(cmd, reply, sizeof(reply));
    if (!r && !strstr(reply, "File selected") &&
        !strstr(reply, "File opened") && !strstr(reply, "Result: ok"))
      r = -1;
  }
  close(l);
  return r;
}
static void print_file(int fd, struct request *r, const char *name) {
  for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
    if (!isalnum(*p) && *p != ' ' && *p != '.' && *p != '_' && *p != '-') {
      error(fd, 400,
            "Rename this file using letters, numbers, spaces, dots, "
            "underscores or hyphens before printing");
      return;
    }
  }
  if (r->length || !extension(name, ".gx")) {
    error(fd, 400, "Choose a GX file to print");
    return;
  }
  int lock = lockfile("mutation.lock", 1);
  if (lock < 0) {
    error(fd, 409, "Another file operation is in progress");
    return;
  }
  int in = openat(rootfd, name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  struct stat s, latest;
  struct gx_meta meta;
  uint32_t a, b;
  char id[128], reply[4096];
  if (in < 0 || fstat(in, &s) || !S_ISREG(s.st_mode) ||
      gx_header(in, &meta, &a, &b)) {
    if (in >= 0)
      close(in);
    close(lock);
    error(fd, 400, "Choose a valid GX file to print");
    return;
  }
  close(in);
  identity(id, sizeof(id), &s);
  if (!*r->etag || strcmp(id, r->etag)) {
    close(lock);
    error(fd, 409, "File changed; refresh before printing");
    return;
  }
  if (!ready(reply, sizeof(reply))) {
    close(lock);
    error(fd, 409, "Printer is busy or its state is unavailable");
    return;
  }
  char latest_id[128];
  if (fstatat(rootfd, name, &latest, AT_SYMLINK_NOFOLLOW)) {
    close(lock);
    error(fd, 409, "File changed; refresh before printing");
    return;
  }
  identity(latest_id, sizeof(latest_id), &latest);
  if (strcmp(id, latest_id)) {
    close(lock);
    error(fd, 409, "File changed; refresh before printing");
    return;
  }
  int result = start_print(name);
  if (!result) {
    char pending[256], pending_id[128], path[1024];
    if (notification_read(pending, sizeof(pending), pending_id,
                          sizeof(pending_id)) &&
        !strcmp(name, pending) && !strcmp(id, pending_id)) {
      snprintf(path, sizeof(path), "%s/upload-notification", state);
      unlink(path);
    }
  }
  close(lock);
  if (result)
    error(fd, 502,
          "The printer did not acknowledge the start. Check its screen before "
          "retrying");
  else
    json(fd, 200, "{\"started\":true}");
}
static void upload(int fd, struct request *r) {
  int mutation = lockfile("mutation.lock", 1);
  if (mutation < 0) {
    error(fd, 409, "Another file operation is in progress");
    return;
  }
  int body = -1, out = -1;
  unsigned char *map = MAP_FAILED;
  char temp[80] = "", message[256] = "Conversion failed";
  int code = 400;
  struct upload u = {0};
  char ready_reply[4096];
  if (!ready(ready_reply, sizeof(ready_reply))) {
    snprintf(message, sizeof(message),
             "Printer is busy or its state is unavailable");
    code = 409;
    goto fail;
  }
  struct statvfs v;
  if (fstatvfs(rootfd, &v) || (uint64_t)v.f_bavail * v.f_frsize <
                                  2 * (uint64_t)r->length + 16 * 1024 * 1024) {
    code = 507;
    snprintf(message, sizeof(message), "Not enough free storage");
    goto fail;
  }
  if (!r->length || !strstr(r->type, "multipart/form-data")) {
    snprintf(message, sizeof(message), "Expected a multipart file upload");
    goto fail;
  }
  body = scratch();
  if (body < 0) {
    code = 500;
    snprintf(message, sizeof(message), "Cannot create temporary input");
    goto fail;
  }
  if (*r->expect) {
    if (strcasecmp(r->expect, "100-continue")) {
      code = 417;
      goto fail;
    }
    sendall(fd, "HTTP/1.1 100 Continue\r\n\r\n", 25);
  }
  size_t left = r->length;
  unsigned char buf[32768];
  while (left) {
    ssize_t n = recv(fd, buf, left < sizeof(buf) ? left : sizeof(buf), 0);
    if (n <= 0) {
      snprintf(message, sizeof(message), "Upload interrupted");
      goto fail;
    }
    if (write(body, buf, n) != n) {
      code = 507;
      snprintf(message, sizeof(message), "Could not store upload");
      goto fail;
    }
    left -= n;
  }
  map = mmap(NULL, r->length, PROT_READ, MAP_PRIVATE, body, 0);
  if (map == MAP_FAILED) {
    code = 500;
    goto fail;
  }
  if (multipart(map, r->length, r->type, &u)) {
    snprintf(message, sizeof(message),
             "Malformed upload or unsupported filename");
    goto fail;
  }
  /* Match the toolkit's safe basename alphabet before handing paths back to
   * stock firmware, whose older handlers can use shell command strings. */
  size_t safe = 0;
  for (size_t i = 0; u.name[i]; i++) {
    unsigned char c = u.name[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_'))
      c = '_';
    if (c == '_' && safe && u.name[safe - 1] == '_')
      continue;
    u.name[safe++] = c;
  }
  u.name[safe] = 0;
  int converted = extension(u.name, ".gcode");
  if (converted) {
    char *dot = strrchr(u.name, '.');
    strcpy(dot, ".gx");
  }
  unsigned char random[8];
  int randomfd = open("/dev/urandom", O_RDONLY);
  if (randomfd < 0 ||
      read(randomfd, random, sizeof(random)) != sizeof(random)) {
    if (randomfd >= 0)
      close(randomfd);
    code = 500;
    goto fail;
  }
  close(randomfd);
  snprintf(temp, sizeof(temp), "output-%ld-%02x%02x%02x%02x.part",
           (long)getpid(), random[0], random[1], random[2], random[3]);
  memcpy(cleanup_output, temp, strlen(temp) + 1);
  out = openat(stagefd, temp,
               O_CREAT | O_EXCL | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0600);
  if (out < 0) {
    code = 500;
    goto fail;
  }
  if (converted) {
    if (gx_convert(body, u.offset, u.length, out, message, sizeof(message)))
      goto fail;
  } else {
    size_t at = u.offset, left = u.length;
    while (left) {
      size_t n = left < sizeof(buf) ? left : sizeof(buf);
      ssize_t got = pread(body, buf, n, at);
      if (got <= 0 || write(out, buf, got) != got) {
        code = 507;
        goto fail;
      }
      at += got;
      left -= got;
    }
  }
  struct gx_meta meta;
  uint32_t b, c;
  if (gx_header(out, &meta, &b, &c)) {
    snprintf(message, sizeof(message), "Invalid GX output");
    goto fail;
  }
  munmap(map, r->length);
  map = MAP_FAILED;
  close(body);
  body = -1; /* Source is discarded before publication. */
  if (!ready(ready_reply, sizeof(ready_reply))) {
    code = 409;
    snprintf(message, sizeof(message),
             "Printer became busy; file was not published");
    goto fail;
  }
  char peek;
  ssize_t connected = recv(fd, &peek, 1, MSG_PEEK | MSG_DONTWAIT);
  if (connected == 0 ||
      (connected < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
    snprintf(message, sizeof(message), "Upload cancelled before publication");
    goto fail;
  }
  if (fchmod(out, 0644) || fsync(out)) {
    code = 507;
    goto fail;
  }
  close(out);
  out = -1;
  struct stat old;
  if (!fstatat(rootfd, u.name, &old, AT_SYMLINK_NOFOLLOW) &&
      !S_ISREG(old.st_mode)) {
    code = 409;
    snprintf(message, sizeof(message),
             "Destination is not a regular print file");
    goto fail;
  }
  if (renameat(stagefd, temp, rootfd, u.name)) {
    code = 500;
    goto fail;
  }
  temp[0] = 0;
  fsync(rootfd);
  int printed = u.print ? start_print(u.name) : 0;
  if (!u.print || printed)
    notify_upload(u.name);
  char *reply = NULL;
  size_t size = 0;
  FILE *f = open_memstream(&reply, &size);
  fprintf(f, "{\"done\":true,\"converted\":%s,\"files\":{\"local\":{\"name\":",
          converted ? "true" : "false");
  quote(f, u.name);
  fprintf(f, ",\"path\":");
  quote(f, u.name);
  fprintf(f, ",\"origin\":\"local\"}},\"printed\":%s,\"printError\":%s}",
          u.print && !printed ? "true" : "false",
          printed ? "\"File saved, but starting the print failed\"" : "null");
  fclose(f);
  response(fd, printed ? 502 : 201, "application/json", reply, size);
  free(reply);
  close(mutation);
  return;
fail:
  if (map != MAP_FAILED)
    munmap(map, r->length);
  if (body >= 0)
    close(body);
  if (out >= 0)
    close(out);
  if (*temp)
    unlinkat(stagefd, temp, 0);
  cleanup_output[0] = 0;
  close(mutation);
  error(fd, code, message);
}
static void delete_file(int fd, struct request *r, const char *name) {
  int lock = lockfile("mutation.lock", 1);
  if (lock < 0) {
    error(fd, 409, "Another file operation is in progress");
    return;
  }
  struct stat s;
  char id[128], reply[4096];
  if (fstatat(rootfd, name, &s, AT_SYMLINK_NOFOLLOW) || !S_ISREG(s.st_mode)) {
    close(lock);
    error(fd, 404, "Print file not found");
    return;
  }
  identity(id, sizeof(id), &s);
  if (!*r->etag || strcmp(id, r->etag)) {
    close(lock);
    error(fd, 409, "File changed; refresh the list before deleting");
    return;
  }
  if (!ready(reply, sizeof(reply))) {
    close(lock);
    error(fd, 409, "Printer is busy or its state is unavailable");
    return;
  }
  struct stat latest;
  char latest_id[128];
  if (fstatat(rootfd, name, &latest, AT_SYMLINK_NOFOLLOW)) {
    close(lock);
    error(fd, 409, "File changed; refresh before deleting");
    return;
  }
  identity(latest_id, sizeof(latest_id), &latest);
  if (strcmp(id, latest_id)) {
    close(lock);
    error(fd, 409, "File changed; refresh before deleting");
    return;
  }
  int result = unlinkat(rootfd, name, 0);
  fsync(rootfd);
  close(lock);
  if (result)
    error(fd, 500, "Could not delete file");
  else
    response(fd, 204, "application/json", "", 0);
}
static void static_file(int fd, const char *path) {
  const char *name = !strcmp(path, "/") ? "index.html" : path + 1;
  const char *mime = !strcmp(name, "index.html")    ? "text/html; charset=utf-8"
                     : !strcmp(name, "app.js")      ? "text/javascript"
                     : !strcmp(name, "style.css")   ? "text/css"
                     : !strcmp(name, "favicon.svg") ? "image/svg+xml"
                     : !strcmp(name, "inter.woff2") ? "font/woff2"
                     : !strcmp(name, "FONT-LICENSE.txt") ? "text/plain"
                                                         : NULL;
  if (!mime) {
    error(fd, 404, "Not found");
    return;
  }
  int dir = open(www, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  int in = dir < 0 ? -1 : openat(dir, name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (dir >= 0)
    close(dir);
  struct stat s;
  if (in < 0 || fstat(in, &s) || s.st_size > 1024 * 1024) {
    if (in >= 0)
      close(in);
    error(fd, 404, "Not found");
    return;
  }
  char *data = malloc(s.st_size ? s.st_size : 1);
  if (!data || read(in, data, s.st_size) != s.st_size) {
    close(in);
    free(data);
    error(fd, 500, "Cannot read asset");
    return;
  }
  close(in);
  response(fd, 200, mime, data, s.st_size);
  free(data);
}
static void handle(int fd) {
  struct request r = {0};
  struct timeval tv = {30, 0};
  setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
  if (get_headers(fd, &r)) {
    error(fd, 400, "Malformed or oversized request");
    return;
  }
  if (!origin_ok(&r)) {
    error(fd, 403, "Cross-origin request rejected");
    return;
  }
  int get = !strcmp(r.method, "GET");
  if (!get && !equalkey(r.api)) {
    error(fd, 403, "API key required");
    return;
  }
  if (get && !strcmp(r.path, "/api/version")) {
    json(fd, 200,
         "{\"api\":\"0.1\",\"server\":\"1.3.10\",\"text\":\"OctoPrint "
         "compatible Finder "
         "Revival\"}");
    return;
  }
  if (get && !strcmp(r.path, "/api/session")) {
    char b[128];
    snprintf(b, sizeof(b), "{\"apiKey\":\"%s\"}", key);
    json(fd, 200, b);
    return;
  }
  if (get && !strcmp(r.path, "/api/status")) {
    status(fd);
    return;
  }
  if (get && !strcmp(r.path, "/api/notifications")) {
    notifications(fd);
    return;
  }
  if (!strcmp(r.method, "POST") &&
      !strcmp(r.path, "/api/notifications/dismiss")) {
    if (r.length)
      error(fd, 400, "Unexpected request body");
    else
      dismiss_notification(fd, r.etag);
    return;
  }
  if (get && !strcmp(r.path, "/api/printer")) {
    printer_info(fd);
    return;
  }
  if (get && (!strcmp(r.path, "/api/camera/stream") ||
              !strcmp(r.path, "/?action=stream"))) {
    camera(fd, 1);
    return;
  }
  if (get && (!strcmp(r.path, "/api/camera/snapshot") ||
              !strcmp(r.path, "/?action=snapshot"))) {
    camera(fd, 0);
    return;
  }
  const char *print_prefix = "/api/print/";
  if (!strcmp(r.method, "POST") &&
      !strncmp(r.path, print_prefix, strlen(print_prefix))) {
    char name[256];
    if (unescape(r.path + strlen(print_prefix), name, sizeof(name)) ||
        !filename(name))
      error(fd, 400, "Invalid print filename");
    else
      print_file(fd, &r, name);
    return;
  }
  if (get &&
      (!strcmp(r.path, "/api/files") || !strcmp(r.path, "/api/files/local"))) {
    files(fd);
    return;
  }
  if (!strcmp(r.method, "POST") &&
      (!strcmp(r.path, "/api/files/local") || !strcmp(r.path, "/upload"))) {
    upload(fd, &r);
    return;
  }
  const char *prefix = "/api/files/local/";
  if (!strncmp(r.path, prefix, strlen(prefix))) {
    char name[256];
    if (unescape(r.path + strlen(prefix), name, sizeof(name)) ||
        !filename(name)) {
      error(fd, 400, "Invalid print filename");
      return;
    }
    if (!strcmp(r.method, "DELETE")) {
      delete_file(fd, &r, name);
      return;
    }
  }
  prefix = "/api/preview/";
  if (get && !strncmp(r.path, prefix, strlen(prefix))) {
    char name[256];
    if (unescape(r.path + strlen(prefix), name, sizeof(name)) ||
        !filename(name)) {
      error(fd, 400, "Invalid print filename");
      return;
    }
    preview(fd, name);
    return;
  }
  if (get)
    static_file(fd, r.path);
  else
    error(fd, 405, "Method not supported");
}
static void reap(int signal_number) {
  (void)signal_number;
  int saved = errno;
  while (waitpid(-1, NULL, WNOHANG) > 0)
    if (children)
      children--;
  errno = saved;
}
int main(int argc, char **argv) {
  if (argc == 4 && !strcmp(argv[1], "--convert")) {
    int in = open(argv[2], O_RDONLY),
        out = open(argv[3], O_CREAT | O_EXCL | O_RDWR, 0600);
    struct stat s;
    char error[256] = "Conversion failed";
    if (in < 0 || out < 0 || fstat(in, &s)) {
      perror("convert");
      return 1;
    }
    int r = gx_convert(in, 0, s.st_size, out, error, sizeof(error));
    close(in);
    close(out);
    if (r) {
      unlink(argv[3]);
      fprintf(stderr, "%s\n", error);
    }
    return !!r;
  }
  for (int i = 1; i < argc; i += 2) {
    if (i + 1 >= argc)
      return 1;
    if (!strcmp(argv[i], "--root"))
      root = argv[i + 1];
    else if (!strcmp(argv[i], "--www"))
      www = argv[i + 1];
    else if (!strcmp(argv[i], "--state"))
      state = argv[i + 1];
    else if (!strcmp(argv[i], "--key-file"))
      keyfile = argv[i + 1];
    else if (!strcmp(argv[i], "--port"))
      port = atoi(argv[i + 1]);
    else if (!strcmp(argv[i], "--printer-port"))
      printer_port = atoi(argv[i + 1]);
    else if (!strcmp(argv[i], "--camera-port"))
      camera_port = atoi(argv[i + 1]);
    else if (!strcmp(argv[i], "--printer-host"))
      snprintf(printer_host, sizeof(printer_host), "%s", argv[i + 1]);
    else if (!strcmp(argv[i], "--bind"))
      snprintf(bind_host, sizeof(bind_host), "%s", argv[i + 1]);
    else
      return 1;
  }
  if (port < 1 || port > 65535 || printer_port < 1 || printer_port > 65535 ||
      camera_port < 1 || camera_port > 65535 || camera_port == port)
    return 1;
  umask(077);
  mkdir(state, 0700);
  int singleton = lockfile("server.lock", 1);
  if (singleton < 0) {
    fprintf(stderr, "Another server is active\n");
    return 1;
  }
  rootfd = open(root, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (rootfd < 0)
    return 1;
  if (mkdirat(rootfd, ".revival-web", 0700) && errno != EEXIST)
    return 1;
  stagefd = openat(rootfd, ".revival-web",
                   O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (stagefd < 0)
    return 1;
  /* Clear only this service's private output/input remnants, never print files.
   */
  DIR *staged = fdopendir(openat(stagefd, ".", O_RDONLY | O_DIRECTORY));
  struct dirent *entry;
  if (staged) {
    while ((entry = readdir(staged)))
      if (!strncmp(entry->d_name, "output-", 7) ||
          !strncmp(entry->d_name, "input-", 6))
        unlinkat(stagefd, entry->d_name, 0);
    closedir(staged);
  }
  int k = open(keyfile, O_RDONLY | O_NOFOLLOW);
  if (k < 0) {
    unsigned char bytes[32];
    int rng = open("/dev/urandom", O_RDONLY);
    if (rng < 0 || read(rng, bytes, 32) != 32)
      return 1;
    close(rng);
    for (int i = 0; i < 32; i++)
      snprintf(key + i * 2, 3, "%02x", bytes[i]);
    k = open(keyfile, O_CREAT | O_EXCL | O_WRONLY | O_NOFOLLOW, 0600);
    if (k < 0 || write(k, key, 64) != 64)
      return 1;
    close(k);
  } else {
    if (read(k, key, 64) != 64)
      return 1;
    close(k);
    key[64] = 0;
  }
  struct rlimit core = {0, 0};
  setrlimit(RLIMIT_CORE, &core);
  signal(SIGPIPE, SIG_IGN);
  struct sigaction sa = {0};
  sa.sa_handler = reap;
  sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
  sigaction(SIGCHLD, &sa, NULL);
  int server = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0), one = 1;
  setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
  struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(port)};
  if (inet_pton(AF_INET, bind_host, &addr.sin_addr) != 1 ||
      bind(server, (struct sockaddr *)&addr, sizeof(addr)) ||
      listen(server, 8)) {
    perror("listen");
    return 1;
  }
  fprintf(stderr, "Finder Revival HTTP listening on %s:%d\n", bind_host, port);
  while (1) {
    int fd = accept4(server, NULL, NULL, SOCK_CLOEXEC);
    if (fd < 0)
      continue;
    if (children >= 4) {
      error(fd, 503, "Server is busy; try again");
      close(fd);
      continue;
    }
    sigset_t blocked, old;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGCHLD);
    sigprocmask(SIG_BLOCK, &blocked, &old);
    pid_t parent = getpid(), child = fork();
    if (child == 0) {
      signal(SIGTERM, worker_exit);
      signal(SIGINT, worker_exit);
      signal(SIGALRM, worker_exit);
      signal(SIGABRT, worker_exit);
      signal(SIGSEGV, worker_exit);
      prctl(PR_SET_PDEATHSIG, SIGTERM);
      if (getppid() != parent)
        _exit(1);
      alarm(180);
      sigprocmask(SIG_SETMASK, &old, NULL);
      close(server);
      close(singleton);
      handle(fd);
      close(fd);
      _exit(0);
    }
    if (child > 0)
      children++;
    sigprocmask(SIG_SETMASK, &old, NULL);
    close(fd);
  }
}
