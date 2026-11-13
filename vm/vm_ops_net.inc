/* vm_ops_net.inc — potongan switch run() di kopivm.c. */

        case OP_NET: {
            uint16_t bi = (uint16_t)m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
            f->pc += 2;
            /* array byte-as-int: [0]=len, [1+i]=elemen (semantik byte 0..255) */
            if (bi == 0) {                        /* socket(domain, type) */
                int64_t tipe = stackv[--sp], dom = stackv[--sp];
                int fd = (int)socket((int)dom, (int)tipe, 0);
                stackv[sp++] = fd;
            } else if (bi == 1) {                 /* connect(fd, ipBE, port) */
                int64_t port = stackv[--sp], ipbe = stackv[--sp], fdi = stackv[--sp];
                struct sockaddr_in sa;
                memset(&sa, 0, sizeof sa);
                sa.sin_family = AF_INET;
                /* builtin menerima nilai polos: ipBE = a*2^24+b*2^16+c*2^8+d,
                 * port = nomor host (mis. 9901); htonl/htons mengurusi urutan
                 * byte jaringan meski endian host apa pun */
                sa.sin_port = htons((uint16_t)((long)port & 0xffff));
                sa.sin_addr.s_addr = htonl((uint32_t)((long)ipbe & 0xffffffff));
                int r = (int)connect((int)fdi, (struct sockaddr *)&sa, sizeof sa);
                stackv[sp++] = (r < 0) ? -errno : r;
            } else if (bi == 2) {                 /* rdb(fd) → byte 0..255 atau -1 */
                int64_t fdi = stackv[--sp];
                unsigned char b;
                long nrd = read((int)fdi, &b, 1);
                if (nrd < 0) dievm("Net.rdb: read gagal");
                stackv[sp++] = (nrd == 0) ? -1 : (long)b;
            } else if (bi == 3) {                 /* wrb(fd, byte) → 0 atau -errno */
                int64_t v = stackv[--sp], fdi = stackv[--sp];
                unsigned char b = (unsigned char)(v & 0xff);
                long nwr = write((int)fdi, &b, 1);
                stackv[sp++] = (nwr == 1) ? 0 : (nwr < 0 ? -errno : -1);
            } else if (bi == 4) {                 /* close(fd) */
                int64_t fdi = stackv[--sp];
                stackv[sp++] = close((int)fdi);
            } else if (bi == 5) {                 /* Net.bytes("literal") → String packed (v1.18) */
                uint16_t cp16 = (uint16_t)m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
                f->pc += 2;
                stackv[sp++] = mk_str_arr(cpool[cp16]);
            } else if (bi == 40) {                /* Net.mkstr(int[] bytes) → String packed */
                long *src = (long *)stackv[--sp];
                if (!src || !in_heap(src))
                    dievm("Net.mkstr: sumber bukan array valid");
                long n = src[0];
                long slots = (n + 7) >> 3;
                if (heap_used + slots + 1 > heap_cap) oom_arena();
                long *blk = heap + heap_used;
                heap_used += slots + 1;
                blk[0] = n;
                for (long k = 0; k < slots; k++) blk[1 + k] = 0;
                for (long i = 0; i < n; i++) {
                    long b = src[1 + i];
                    if (b < 0 || b > 255) dievm2("Net.mkstr: byte di luar 0..255", b);
                    setb(blk, i, b);
                }
                stackv[sp++] = (int64_t)blk;
            } else if (bi == 41) {                /* Net.strbytes(String) → int[] */
                long *src = (long *)stackv[--sp];
                if (!src || !in_heap(src))
                    dievm("Net.strbytes: sumber bukan string valid");
                long n = src[0];
                if (heap_used + n + 1 > heap_cap) oom_arena();
                long *blk = heap + heap_used;
                heap_used += n + 1;
                blk[0] = n;
                for (long i = 0; i < n; i++) blk[1 + i] = getb(src, i);
                stackv[sp++] = (int64_t)blk;
            } else if (bi == 6) {                 /* Net.openr("literal") → fd */
                uint16_t cp16 = (uint16_t)m->code[f->pc] | ((uint16_t)m->code[f->pc + 1] << 8);
                f->pc += 2;
                int fd = (int)open(cpool[cp16], O_RDONLY);
                stackv[sp++] = (fd < 0) ? (int64_t)-1 : (int64_t)fd;
            } else if (bi == 7) {                 /* recvb(fd, buf) → n | -1 */
                long *buf = (long *)stackv[--sp];
                int64_t fdi = stackv[--sp];
                long len = buf[0];
                long nrd = 0;
                if (len > 0) {
                    unsigned char *tmp = (unsigned char *)malloc((size_t)len);
                    if (!tmp) dievm("Net.recvb: memori habis");
                    nrd = read((int)fdi, tmp, (size_t)len);
                    if (nrd < 0) nrd = -1;
                    for (long i = 0; i < nrd; i++)
                        buf[1 + i] = (long)tmp[i];
                    free(tmp);
                }
                stackv[sp++] = (int64_t)nrd;
            } else if (bi == 8) {                 /* sendb(fd, buf) → 0 | -1 */
                long *buf = (long *)stackv[--sp];
                int64_t fdi = stackv[--sp];
                long len = buf[0];
                long terkirim = 0;
                while (terkirim < len) {
                    unsigned char tmp[4096];
                    long want = len - terkirim;
                    if (want > 4096) want = 4096;
                    for (long i = 0; i < want; i++)
                        tmp[i] = (unsigned char)(buf[1 + terkirim + i] & 0xff);
                    long nwr = send((int)fdi, tmp, (size_t)want, MSG_NOSIGNAL);
                    if (nwr < 0 && errno == ENOTSOCK)
                        nwr = write((int)fdi, tmp, (size_t)want);
                    if (nwr <= 0) { terkirim = -1; break; }
                    terkirim += nwr;
                }
                stackv[sp++] = (terkirim >= 0) ? (int64_t)0 : (int64_t)-1;
            } else if (bi == 9) {                 /* bind(fd, ip, port) */
                int64_t port = stackv[--sp], ipbe = stackv[--sp], fdi = stackv[--sp];
                struct sockaddr_in sa;
                memset(&sa, 0, sizeof sa);
                sa.sin_family = AF_INET;
                sa.sin_port = htons((uint16_t)((long)port & 0xffff));
                sa.sin_addr.s_addr = htonl((uint32_t)((long)ipbe & 0xffffffff));
                int r = bind((int)fdi, (struct sockaddr *)&sa, sizeof sa);
                stackv[sp++] = (r < 0) ? -errno : r;
            } else if (bi == 10) {                /* sendto(fd, ip, port, buf) */
                long *buf = (long *)stackv[--sp];
                int64_t port = stackv[--sp], ipbe = stackv[--sp], fdi = stackv[--sp];
                struct sockaddr_in sa;
                memset(&sa, 0, sizeof sa);
                sa.sin_family = AF_INET;
                sa.sin_port = htons((uint16_t)((long)port & 0xffff));
                sa.sin_addr.s_addr = htonl((uint32_t)((long)ipbe & 0xffffffff));
                long len = buf[0];
                if (len > 1200) len = 1200;       /* batas penyangga v1 */
                unsigned char *tmp = (unsigned char *)malloc((size_t)(len > 0 ? len : 1));
                if (!tmp) dievm("Net.sendto: memori habis");
                for (long i = 0; i < len; i++) tmp[i] = (unsigned char)(buf[1 + i] & 0xff);
                ssize_t n = sendto((int)fdi, tmp, (size_t)len, 0,
                                   (struct sockaddr *)&sa, sizeof sa);
                free(tmp);
                stackv[sp++] = (n < 0) ? -errno : (int64_t)n;
            } else if (bi == 11) {                /* recvfrom(fd, buf, from) */
                long *fromArr = (long *)stackv[--sp];
                long *buf = (long *)stackv[--sp];
                int64_t fdi = stackv[--sp];
                unsigned char tmp[1200];
                struct sockaddr_in sa;
                socklen_t sl = sizeof sa;
                memset(&sa, 0, sizeof sa);
                ssize_t n = recvfrom((int)fdi, tmp, sizeof tmp, 0,
                                     (struct sockaddr *)&sa, &sl);
                if (n < 0) {
                    stackv[sp++] = -errno;
                } else {
                    for (long i = 0; i < n; i++) buf[1 + i] = (long)tmp[i];
                    fromArr[1] = (long)ntohl((uint32_t)sa.sin_addr.s_addr);
                    fromArr[2] = (long)ntohs(sa.sin_port);
                    stackv[sp++] = (int64_t)n;
                }
            } else if (bi == 12) {                /* shutdown(fd, how) */
                int64_t how = stackv[--sp], fdi = stackv[--sp];
                int r = shutdown((int)fdi, (int)how);
                stackv[sp++] = (r < 0) ? -errno : r;
            } else if (bi == 13) {                /* openb(path, flags, mode) */
                int64_t mode = stackv[--sp], flags = stackv[--sp];
                long *pa = (long *)stackv[--sp];
                char *path = pack_cpath(pa, 4000);
                int fd = open(path, (int)flags, (int)mode);
                free(path);
                stackv[sp++] = (fd < 0) ? -errno : fd;
            } else if (bi == 14) {                /* mkdirb(path) */
                long *pa = (long *)stackv[--sp];
                char *path = pack_cpath(pa, 4000);
                int r = mkdir(path, 0755);
                free(path);
                stackv[sp++] = (r < 0) ? -errno : r;
            } else if (bi == 15) {                /* unlinkb(path) */
                long *pa = (long *)stackv[--sp];
                char *path = pack_cpath(pa, 4000);
                int r = unlink(path);
                free(path);
                stackv[sp++] = (r < 0) ? -errno : r;
            } else if (bi == 16) {                /* renameb(old, new) */
                long *na = (long *)stackv[--sp];
                long *oa = (long *)stackv[--sp];
                char *nw = pack_cpath(na, 4000);
                char *od = pack_cpath(oa, 4000);
                int r = rename(od, nw);
                free(od); free(nw);
                stackv[sp++] = (r < 0) ? -errno : r;
            } else if (bi == 17) {                /* seek(fd, off, whence) */
                int64_t whence = stackv[--sp], off = stackv[--sp], fdi = stackv[--sp];
                off_t r = lseek((int)fdi, (off_t)off, (int)whence);
                stackv[sp++] = (r < 0) ? -errno : (int64_t)r;
            } else if (bi == 18) {                /* time() */
                struct timespec ts;
                clock_gettime(CLOCK_REALTIME, &ts);
                stackv[sp++] = (int64_t)ts.tv_sec;
            } else if (bi == 19) {                /* mono() ms */
                struct timespec ts;
                clock_gettime(CLOCK_MONOTONIC, &ts);
                stackv[sp++] = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
            } else if (bi == 20) {                /* sleep(ms) */
                int64_t ms = stackv[--sp];
                struct timespec ts;
                ts.tv_sec = (time_t)(ms / 1000);
                ts.tv_nsec = (long)((ms % 1000) * 1000000);
                int r = nanosleep(&ts, 0);
                stackv[sp++] = (r < 0) ? -errno : 0;
            } else if (bi == 21) {                /* exit(code) */
                int64_t code = stackv[--sp];
                exit((int)code);
            } else if (bi == 22) {                /* cat(a,b) */
                long *b = (long *)stackv[--sp];
                long *a = (long *)stackv[--sp];
                long la = a[0], lb = b[0];
                long total = la + lb;
                long slots = (total + 7) >> 3;
                if (heap_used + slots + 1 > heap_cap) oom_arena();
                long *blk = heap + heap_used;
                heap_used += slots + 1;
                blk[0] = total;
                for (long k = 0; k < slots; k++) blk[1 + k] = 0;
                for (long i = 0; i < la; i++) setb(blk, i, getb(a, i));
                for (long j = 0; j < lb; j++) setb(blk, la + j, getb(b, j));
                stackv[sp++] = (int64_t)blk;
            } else if (bi == 23 || bi == 24) {    /* sequal / sne */
                long *b = (long *)stackv[--sp];
                long *a = (long *)stackv[--sp];
                int eq = (a[0] == b[0]);
                for (long i = 0; eq && i < a[0]; i++)
                    if (getb(a, i) != getb(b, i)) eq = 0;
                if (bi == 24) eq = eq ? 0 : 1;
                stackv[sp++] = eq;
            } else if (bi == 25) {                /* listen(fd, backlog) */
                int64_t backlog = stackv[--sp], fdi = stackv[--sp];
                int r = listen((int)fdi, (int)backlog);
                stackv[sp++] = (r < 0) ? -errno : r;
            } else if (bi == 26) {                /* accept(fd) */
                int64_t fdi = stackv[--sp];
                int fd2 = accept((int)fdi, 0, 0);
                stackv[sp++] = (fd2 < 0) ? -errno : fd2;
            } else if (bi == 27) {                /* setsockopt(fd,lvl,opt,val) */
                int64_t val = stackv[--sp], opt = stackv[--sp];
                int64_t lvl = stackv[--sp], fdi = stackv[--sp];
                int v = (int)val;
                int r = setsockopt((int)fdi, (int)lvl, (int)opt, &v, sizeof v);
                stackv[sp++] = (r < 0) ? -errno : r;
            } else if (bi == 28) {                /* memmark() */
                stackv[sp++] = (int64_t)heap_used;
            } else if (bi == 29) {                /* memreset(mark) */
                int64_t mark = stackv[--sp];
                if (mark < 0) mark = 0;
                if (mark > heap_used) mark = heap_used;
                heap_used = (long)mark;
                stackv[sp++] = 0;
            } else if (bi == 30) {                /* argc() */
                stackv[sp++] = vargc;
            } else if (bi == 31) {                /* argv(i) */
                int64_t i = stackv[--sp];
                if (i >= 0 && i < vargc)
                    stackv[sp++] = mk_str_arr(vargv[i]);
                else
                    stackv[sp++] = mk_str_arr(0);
            } else if (bi == 32) {                /* getenv(name) */
                long *pa = (long *)stackv[--sp];
                char *name = pack_cpath(pa, 4000);
                char *v = getenv(name);
                free(name);
                stackv[sp++] = mk_str_arr(v);
            } else if (bi == 33) {                /* recv1(fd, buf): satu read */
                long *buf = (long *)stackv[--sp];
                int64_t fdi = stackv[--sp];
                long len = buf[0];
                if (len > 4000) len = 4000;
                unsigned char tmp[4000];
                ssize_t n = read((int)fdi, tmp, (size_t)(len > 0 ? len : 1));
                if (n > 0) for (long i = 0; i < n; i++) buf[1 + i] = (long)tmp[i];
                stackv[sp++] = (n < 0) ? -errno : (int64_t)n;
            } else if (bi == 39) {                /* pollfd(fds, revents, ms) */
                int64_t ms = stackv[--sp];
                long *rev = (long *)stackv[--sp];
                long *fds = (long *)stackv[--sp];
                long n = fds[0];
                if (n > 64) n = 64;
                struct pollfd pf[64];
                for (long i = 0; i < n; i++) {
                    pf[i].fd = (int)fds[1 + i];
                    pf[i].events = POLLIN;
                    pf[i].revents = 0;
                }
                int r = poll(pf, (nfds_t)n, (int)ms);
                int cnt = 0;
                for (long i = 0; i < n; i++) {
                    int v = 0;
                    if (pf[i].revents & POLLIN) v |= 1;
                    if (pf[i].revents & (POLLERR | POLLHUP | POLLNVAL)) v |= 2;
                    rev[1 + i] = v;
                    if (pf[i].revents) cnt++;
                }
                stackv[sp++] = (r < 0) ? -errno : cnt;
            } else if (bi == 38) {                /* settimeout(fd, ms) */
                int64_t ms = stackv[--sp], fdi = stackv[--sp];
                struct timeval tv;
                tv.tv_sec = (time_t)(ms / 1000);
                tv.tv_usec = (suseconds_t)((ms % 1000) * 1000);
                int r = setsockopt((int)fdi, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
                if (r == 0)
                    r = setsockopt((int)fdi, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
                stackv[sp++] = (r < 0) ? -errno : 0;
            } else if (bi == 37) {                /* localport(fd) */
                int64_t fdi = stackv[--sp];
                struct sockaddr_in sa;
                socklen_t sl = sizeof sa;
                memset(&sa, 0, sizeof sa);
                int r = getsockname((int)fdi, (struct sockaddr *)&sa, &sl);
                stackv[sp++] = (r < 0) ? -errno : (int64_t)ntohs(sa.sin_port);
            } else if (bi == 34) {                /* setnonblock(fd) */
                int64_t fdi = stackv[--sp];
                int fl = fcntl((int)fdi, F_GETFL, 0);
                int r = (fl < 0) ? -1 : fcntl((int)fdi, F_SETFL, fl | O_NONBLOCK);
                stackv[sp++] = (r < 0) ? -errno : 0;
            } else if (bi == 35) {                /* key(): stdin nonblocking */
                fcntl(0, F_SETFL, fcntl(0, F_GETFL, 0) | O_NONBLOCK);
                unsigned char ch;
                ssize_t n = read(0, &ch, 1);
                stackv[sp++] = (n == 1) ? (int64_t)ch : -1;
            } else if (bi == 36) {                /* ttyRaw(on) */
                int64_t on = stackv[--sp];
                int r = system(on ? "stty raw -echo 2>/dev/null" : "stty sane 2>/dev/null");
                stackv[sp++] = (r == -1) ? -1 : 0;
            } else {
                dievm("Net builtin tidak dikenal");
            }
            break;
        }
