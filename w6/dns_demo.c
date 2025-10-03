// dns_demo.c
// DNS Resolution, Query Types, Caching, and Network Programming — with pause sections
//
// Build:
//   gcc -O0 -g -Wall -Wextra -o dns_demo dns_demo.c -lresolv
// Run:
//   ./dns_demo
//
// Sections (each pauses):
//   1) Basic hostname resolution (getaddrinfo vs deprecated gethostbyname)
//   2) IPv4 vs IPv6 resolution (A vs AAAA records)
//   3) Reverse DNS lookups (PTR records)
//   4) Different DNS record types (MX, TXT, NS)
//   5) DNS caching effects and TTL
//   6) Error handling and timeouts
//   7) /etc/hosts vs DNS server resolution
//
// Notes:
//   • DNS: Domain Name System maps human-readable names to IP addresses
//   • getaddrinfo: modern, protocol-independent address resolution
//   • res_query: lower-level DNS query interface for specific record types
//   • DNS caching: resolver libraries cache results to reduce network traffic
//   • TTL: Time To Live specifies how long a record can be cached

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE

#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <netinet/in.h>
#include <resolv.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>

/* ============================ Utilities ============================ */
static void wait_for_enter(const char *title) {
    if (title && *title) printf("\n===== %s =====\n", title);
    printf("Press ENTER to continue...\n");
    int c; while ((c = getchar()) != '\n' && c != EOF) {}
}

static double get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

/* ============== PART 1: Basic hostname resolution ================= */
// Modern way: getaddrinfo (IPv4/IPv6 agnostic, preferred)
static void resolve_with_getaddrinfo(const char *hostname) {
    printf("\n[getaddrinfo] Resolving '%s'...\n", hostname);
    
    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;     // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    
    double start = get_time_ms();
    int s = getaddrinfo(hostname, NULL, &hints, &result);
    double elapsed = get_time_ms() - start;
    
    if (s != 0) {
        fprintf(stderr, "  ❌ getaddrinfo failed: %s\n", gai_strerror(s));
        return;
    }
    
    printf("  ✅ Resolution took %.2f ms\n", elapsed);
    printf("  Results:\n");
    
    int count = 0;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        char addr_str[INET6_ADDRSTRLEN];
        void *addr_ptr;
        const char *ipver;
        
        if (rp->ai_family == AF_INET) {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)rp->ai_addr;
            addr_ptr = &(ipv4->sin_addr);
            ipver = "IPv4";
        } else if (rp->ai_family == AF_INET6) {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)rp->ai_addr;
            addr_ptr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        } else {
            continue;
        }
        
        inet_ntop(rp->ai_family, addr_ptr, addr_str, sizeof(addr_str));
        printf("    [%d] %s: %s\n", ++count, ipver, addr_str);
    }
    
    freeaddrinfo(result);
}

// Deprecated way: gethostbyname (IPv4 only, not thread-safe)
// ⚠️ Included for educational purposes to show why it's deprecated
static void resolve_with_gethostbyname_DEPRECATED(const char *hostname) {
    printf("\n[gethostbyname - DEPRECATED] Resolving '%s'...\n", hostname);
    printf("  ⚠️  Warning: gethostbyname is deprecated and NOT thread-safe!\n");
    printf("  ⚠️  It only supports IPv4 and uses a static buffer.\n");
    
    struct hostent *he = gethostbyname(hostname);
    if (he == NULL) {
        fprintf(stderr, "  ❌ gethostbyname failed: h_errno=%d\n", h_errno);
        return;
    }
    
    printf("  Official name: %s\n", he->h_name);
    printf("  Address type: %s\n", he->h_addrtype == AF_INET ? "AF_INET (IPv4)" : "Other");
    printf("  Addresses:\n");
    
    for (int i = 0; he->h_addr_list[i] != NULL; i++) {
        char addr_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, he->h_addr_list[i], addr_str, sizeof(addr_str));
        printf("    [%d] %s\n", i + 1, addr_str);
    }
}

/* =========== PART 2: IPv4 vs IPv6 (A vs AAAA records) ============= */
static void resolve_ipv4_only(const char *hostname) {
    printf("\n[IPv4 only - A records] Resolving '%s'...\n", hostname);
    
    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // Force IPv4
    hints.ai_socktype = SOCK_STREAM;
    
    int s = getaddrinfo(hostname, NULL, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "  ❌ Failed: %s\n", gai_strerror(s));
        return;
    }
    
    printf("  IPv4 addresses:\n");
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)rp->ai_addr;
        char addr_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(ipv4->sin_addr), addr_str, sizeof(addr_str));
        printf("    %s\n", addr_str);
    }
    
    freeaddrinfo(result);
}

static void resolve_ipv6_only(const char *hostname) {
    printf("\n[IPv6 only - AAAA records] Resolving '%s'...\n", hostname);
    
    struct addrinfo hints, *result, *rp;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;      // Force IPv6
    hints.ai_socktype = SOCK_STREAM;
    
    int s = getaddrinfo(hostname, NULL, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "  ❌ Failed: %s\n", gai_strerror(s));
        return;
    }
    
    printf("  IPv6 addresses:\n");
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)rp->ai_addr;
        char addr_str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &(ipv6->sin6_addr), addr_str, sizeof(addr_str));
        printf("    %s\n", addr_str);
    }
    
    freeaddrinfo(result);
}

/* =============== PART 3: Reverse DNS (PTR records) ================ */
static void reverse_dns_lookup(const char *ip_str) {
    printf("\n[Reverse DNS - PTR record] Looking up '%s'...\n", ip_str);
    
    struct sockaddr_in sa;
    struct sockaddr_in6 sa6;
    socklen_t len;
    void *addr;
    
    // Try IPv4 first
    if (inet_pton(AF_INET, ip_str, &(sa.sin_addr)) == 1) {
        sa.sin_family = AF_INET;
        addr = &sa;
        len = sizeof(sa);
    } 
    // Try IPv6
    else if (inet_pton(AF_INET6, ip_str, &(sa6.sin6_addr)) == 1) {
        sa6.sin6_family = AF_INET6;
        addr = &sa6;
        len = sizeof(sa6);
    } 
    else {
        fprintf(stderr, "  ❌ Invalid IP address format\n");
        return;
    }
    
    char hostname[NI_MAXHOST];
    double start = get_time_ms();
    int s = getnameinfo(addr, len, hostname, sizeof(hostname), NULL, 0, 0);
    double elapsed = get_time_ms() - start;
    
    if (s != 0) {
        fprintf(stderr, "  ❌ Reverse lookup failed: %s\n", gai_strerror(s));
    } else {
        printf("  ✅ Hostname: %s (took %.2f ms)\n", hostname, elapsed);
    }
}

/* ========== PART 4: Different DNS record types (MX, TXT, NS) ======= */
static void query_mx_records(const char *domain) {
    printf("\n[MX Records - Mail Exchange] Querying '%s'...\n", domain);
    
    unsigned char answer[4096];
    int len = res_query(domain, C_IN, T_MX, answer, sizeof(answer));
    
    if (len < 0) {
        fprintf(stderr, "  ❌ MX query failed: %s\n", hstrerror(h_errno));
        return;
    }
    
    printf("  ✅ Got %d bytes of response data\n", len);
    
    ns_msg msg;
    if (ns_initparse(answer, len, &msg) < 0) {
        fprintf(stderr, "  ❌ Failed to parse response\n");
        return;
    }
    
    int count = ns_msg_count(msg, ns_s_an);
    printf("  Found %d MX record(s):\n", count);
    
    for (int i = 0; i < count; i++) {
        ns_rr rr;
        if (ns_parserr(&msg, ns_s_an, i, &rr) < 0) continue;
        
        if (ns_rr_type(rr) == ns_t_mx) {
            const unsigned char *rdata = ns_rr_rdata(rr);
            unsigned short pref = ns_get16(rdata);
            
            char mx_name[MAXDNAME];
            if (ns_name_uncompress(answer, answer + len, rdata + 2, 
                                   mx_name, sizeof(mx_name)) < 0) {
                strcpy(mx_name, "<parse error>");
            }
            
            printf("    Priority %u: %s\n", pref, mx_name);
        }
    }
}

static void query_txt_records(const char *domain) {
    printf("\n[TXT Records - Text] Querying '%s'...\n", domain);
    
    unsigned char answer[4096];
    int len = res_query(domain, C_IN, T_TXT, answer, sizeof(answer));
    
    if (len < 0) {
        fprintf(stderr, "  ❌ TXT query failed: %s\n", hstrerror(h_errno));
        return;
    }
    
    printf("  ✅ Got %d bytes of response data\n", len);
    
    ns_msg msg;
    if (ns_initparse(answer, len, &msg) < 0) {
        fprintf(stderr, "  ❌ Failed to parse response\n");
        return;
    }
    
    int count = ns_msg_count(msg, ns_s_an);
    printf("  Found %d TXT record(s):\n", count);
    
    for (int i = 0; i < count; i++) {
        ns_rr rr;
        if (ns_parserr(&msg, ns_s_an, i, &rr) < 0) continue;
        
        if (ns_rr_type(rr) == ns_t_txt) {
            const unsigned char *rdata = ns_rr_rdata(rr);
            unsigned int rdlen = ns_rr_rdlen(rr);
            
            printf("    \"");
            unsigned int pos = 0;
            while (pos < rdlen) {
                unsigned char txt_len = rdata[pos++];
                for (unsigned char j = 0; j < txt_len && pos < rdlen; j++, pos++) {
                    printf("%c", rdata[pos]);
                }
            }
            printf("\"\n");
        }
    }
}

/* ============== PART 5: DNS caching effects and TTL =============== */
static void demonstrate_caching(const char *hostname) {
    printf("\n[DNS Caching] Multiple lookups of '%s'...\n", hostname);
    printf("  Note: System resolver caches results; repeated lookups are faster.\n");
    
    for (int i = 1; i <= 3; i++) {
        printf("\n  Lookup #%d:\n", i);
        
        struct addrinfo hints, *result;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;
        
        double start = get_time_ms();
        int s = getaddrinfo(hostname, NULL, &hints, &result);
        double elapsed = get_time_ms() - start;
        
        if (s == 0) {
            char addr_str[INET6_ADDRSTRLEN];
            void *addr_ptr;
            
            if (result->ai_family == AF_INET) {
                addr_ptr = &((struct sockaddr_in *)result->ai_addr)->sin_addr;
            } else {
                addr_ptr = &((struct sockaddr_in6 *)result->ai_addr)->sin6_addr;
            }
            
            inet_ntop(result->ai_family, addr_ptr, addr_str, sizeof(addr_str));
            printf("    ✅ Resolved to %s in %.2f ms", addr_str, elapsed);
            
            if (i == 1) {
                printf(" (initial - may hit DNS server)\n");
            } else {
                printf(" (likely cached)\n");
            }
            
            freeaddrinfo(result);
        } else {
            fprintf(stderr, "    ❌ Failed: %s\n", gai_strerror(s));
        }
        
        if (i < 3) usleep(100000); // 100ms delay
    }
}

/* ============ PART 6: Error handling and timeouts ================= */
static void demonstrate_errors(void) {
    printf("\n[DNS Error Handling] Testing various error conditions...\n");
    
    // 1. Non-existent domain
    printf("\n  Test 1: Non-existent domain\n");
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    
    const char *bad_domain = "this-domain-definitely-does-not-exist-12345.invalid";
    int s = getaddrinfo(bad_domain, NULL, &hints, &result);
    if (s != 0) {
        printf("    ✅ Correctly failed with: %s\n", gai_strerror(s));
        printf("    (EAI_NONAME or EAI_AGAIN expected for non-existent domains)\n");
    } else {
        printf("    ❌ Unexpectedly succeeded?\n");
        freeaddrinfo(result);
    }
    
    // 2. Invalid hostname format
    printf("\n  Test 2: Invalid hostname format\n");
    const char *invalid = "-.invalid.-";
    s = getaddrinfo(invalid, NULL, &hints, &result);
    if (s != 0) {
        printf("    ✅ Correctly failed with: %s\n", gai_strerror(s));
    } else {
        printf("    Resolved (resolver may be lenient)\n");
        freeaddrinfo(result);
    }
    
    // 3. NULL hostname
    printf("\n  Test 3: NULL hostname\n");
    s = getaddrinfo(NULL, "80", &hints, &result);
    if (s == 0) {
        printf("    ⚠️  NULL hostname allowed when service is specified\n");
        printf("    (returns wildcard addresses for binding)\n");
        freeaddrinfo(result);
    } else {
        printf("    Failed with: %s\n", gai_strerror(s));
    }
}

/* ========= PART 7: /etc/hosts vs DNS server resolution ============ */
static void demonstrate_hosts_file(void) {
    printf("\n[/etc/hosts vs DNS] Resolution order...\n");
    printf("  The resolver typically checks /etc/hosts before DNS servers.\n");
    printf("  Configuration in /etc/nsswitch.conf determines order.\n\n");
    
    // localhost should always resolve via /etc/hosts
    printf("  Test 1: 'localhost' (should be in /etc/hosts)\n");
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    
    double start = get_time_ms();
    int s = getaddrinfo("localhost", NULL, &hints, &result);
    double elapsed = get_time_ms() - start;
    
    if (s == 0) {
        char addr_str[INET6_ADDRSTRLEN];
        void *addr_ptr;
        int family = result->ai_family;
        
        if (family == AF_INET) {
            addr_ptr = &((struct sockaddr_in *)result->ai_addr)->sin_addr;
        } else {
            addr_ptr = &((struct sockaddr_in6 *)result->ai_addr)->sin6_addr;
        }
        
        inet_ntop(family, addr_ptr, addr_str, sizeof(addr_str));
        printf("    ✅ Resolved to %s in %.2f ms (very fast = /etc/hosts)\n", 
               addr_str, elapsed);
        freeaddrinfo(result);
    }
    
    printf("\n  Typical /etc/nsswitch.conf entry:\n");
    printf("    hosts: files dns\n");
    printf("    (files = /etc/hosts, dns = DNS servers)\n");
}

/* ============================= Driver ============================= */
int main(void) {
    // Initialize resolver
    res_init();
    
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║          DNS Resolution Demo - Educational Tool              ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    /* -------------------- Part 1: Basic resolution ------------------- */
    printf("\n=== Part 1: Basic Hostname Resolution ===\n");
    printf("Modern programs should use getaddrinfo (protocol-independent).\n");
    printf("Old code may use gethostbyname (IPv4-only, deprecated).\n");
    
    resolve_with_getaddrinfo("www.google.com");
    resolve_with_gethostbyname_DEPRECATED("www.google.com");
    
    wait_for_enter("Discuss: Why is getaddrinfo preferred over gethostbyname?");
    
    printf("\n📖 ANSWER:\n");
    printf("   • Protocol independence: getaddrinfo supports both IPv4 and IPv6, while\n");
    printf("     gethostbyname only supports IPv4. Modern networks need dual-stack support.\n\n");
    printf("   • Thread safety: gethostbyname uses a static buffer (NOT thread-safe!).\n");
    printf("     Multiple threads can overwrite each other's results.\n");
    printf("     getaddrinfo allocates memory per-call (caller must free it).\n\n");
    printf("   • Modern standard: getaddrinfo is POSIX standard since 2001.\n");
    printf("     gethostbyname is deprecated and may be removed in future systems.\n\n");
    printf("   • Flexibility: getaddrinfo returns ready-to-use sockaddr structures\n");
    printf("     and allows filtering by socket type and protocol.\n");
    
    /* -------------- Part 2: IPv4 vs IPv6 (A vs AAAA) ---------------- */
    printf("\n=== Part 2: IPv4 vs IPv6 Resolution ===\n");
    printf("A records: IPv4 addresses (32-bit)\n");
    printf("AAAA records: IPv6 addresses (128-bit)\n");
    
    resolve_ipv4_only("www.google.com");
    resolve_ipv6_only("www.google.com");
    
    wait_for_enter("Discuss: What's the difference between A and AAAA records? Dual-stack?");
    
    printf("\n📖 ANSWER:\n");
    printf("   • A records: Return IPv4 addresses (32-bit, e.g., 142.250.185.78)\n");
    printf("     Format: dotted decimal (4 octets)\n\n");
    printf("   • AAAA records: Return IPv6 addresses (128-bit, e.g., 2607:f8b0:4004:c07::6a)\n");
    printf("     Format: colon-hexadecimal (8 groups of 16 bits)\n");
    printf("     Name: 'AAAA' because IPv6 is 4 times larger than IPv4 ('A')\n\n");
    printf("   • Dual-stack: Systems supporting both IPv4 and IPv6 simultaneously.\n");
    printf("     Using AF_UNSPEC with getaddrinfo gets both A and AAAA records,\n");
    printf("     allowing the application to choose (Happy Eyeballs: try IPv6 first,\n");
    printf("     fall back to IPv4 if it fails).\n\n");
    printf("   • Why both?: IPv4 address exhaustion requires IPv6 migration, but IPv4\n");
    printf("     remains ubiquitous, so most services support both for compatibility.\n");
    
    /* -------------- Part 3: Reverse DNS (PTR records) ---------------- */
    printf("\n=== Part 3: Reverse DNS Lookups ===\n");
    printf("PTR records map IP addresses back to hostnames.\n");
    printf("Used for logging, spam filtering, and verification.\n");
    
    reverse_dns_lookup("8.8.8.8");        // Google DNS
    reverse_dns_lookup("1.1.1.1");        // Cloudflare DNS
    
    wait_for_enter("Discuss: When is reverse DNS useful? Why might it fail?");
    
    printf("\n📖 ANSWER:\n");
    printf("   Use cases for reverse DNS:\n");
    printf("   • Logging: Convert IP addresses in logs to readable hostnames\n");
    printf("   • Email: SMTP servers check reverse DNS to verify sender legitimacy\n");
    printf("   • Security: Verify that forward and reverse DNS match (FCrDNS check)\n");
    printf("   • Troubleshooting: Identify what host an IP belongs to\n\n");
    printf("   Why it might fail:\n");
    printf("   • PTR record not configured: Many hosts (especially clients) don't have\n");
    printf("     reverse DNS set up. It's optional and requires ISP/admin configuration.\n");
    printf("   • Timeout: DNS server for the IP's reverse zone may be unreachable\n");
    printf("   • Delegation: Reverse DNS requires proper delegation of in-addr.arpa or\n");
    printf("     ip6.arpa zones, which may not be set up correctly\n\n");
    printf("   ⚠️  Security note: Reverse DNS can be controlled by whoever owns the IP,\n");
    printf("   so it's not cryptographically secure. Don't rely on it for authentication!\n");
    
    /* ---------- Part 4: Different record types (MX, TXT, NS) --------- */
    printf("\n=== Part 4: Different DNS Record Types ===\n");
    printf("DNS supports many record types beyond A/AAAA:\n");
    printf("  MX: Mail exchange servers (email routing)\n");
    printf("  TXT: Arbitrary text (SPF, DKIM, verification)\n");
    printf("  NS: Name servers (delegation)\n");
    
    query_mx_records("gmail.com");
    query_txt_records("google.com");
    
    wait_for_enter("Discuss: What are MX records used for? What about TXT records?");
    
    printf("\n📖 ANSWER:\n");
    printf("   MX (Mail Exchange) records:\n");
    printf("   • Purpose: Specify mail servers that accept email for a domain\n");
    printf("   • Priority: Lower numbers = higher priority (try first)\n");
    printf("   • Example: gmail.com → gmail-smtp-in.l.google.com (priority 5)\n");
    printf("   • When you send email to user@example.com, your mail server queries\n");
    printf("     MX records for example.com to find where to deliver the message\n");
    printf("   • Multiple MX records provide redundancy and load balancing\n\n");
    printf("   TXT (Text) records:\n");
    printf("   • Purpose: Store arbitrary text data, widely used for:\n");
    printf("     - SPF (Sender Policy Framework): List IPs authorized to send email\n");
    printf("       Example: 'v=spf1 include:_spf.google.com ~all'\n");
    printf("     - DKIM (DomainKeys Identified Mail): Public keys for email signing\n");
    printf("     - Domain verification: Prove you own a domain (Google, Let's Encrypt)\n");
    printf("     - DMARC: Email authentication policies\n");
    printf("   • Originally for human-readable notes, now mostly machine-readable config\n\n");
    printf("   Other important record types:\n");
    printf("   • NS: Delegate a subdomain to other nameservers\n");
    printf("   • CNAME: Alias one name to another (canonical name)\n");
    printf("   • SRV: Service location (port, weight, priority) for protocols\n");
    
    /* -------------- Part 5: DNS caching and TTL ---------------------- */
    printf("\n=== Part 5: DNS Caching Effects ===\n");
    printf("Resolvers cache DNS results to reduce network traffic.\n");
    printf("TTL (Time To Live) controls how long records can be cached.\n");
    
    demonstrate_caching("www.example.com");
    
    wait_for_enter("Discuss: Why is DNS caching important? What are the tradeoffs?");
    
    printf("\n📖 ANSWER:\n");
    printf("   Benefits of DNS caching:\n");
    printf("   • Performance: Avoid network round-trip for repeated queries.\n");
    printf("     First lookup may take 20-100ms, cached lookups take <1ms\n");
    printf("   • Scalability: Reduces load on authoritative DNS servers.\n");
    printf("     Without caching, root and TLD servers would be overwhelmed\n");
    printf("   • Reliability: If authoritative server is down, cached results still work\n");
    printf("     until TTL expiry\n");
    printf("   • Cost: DNS queries consume bandwidth and may have $ costs\n\n");
    printf("   How it works:\n");
    printf("   • TTL (Time To Live): Each DNS record has a TTL (e.g., 300 seconds = 5 min)\n");
    printf("     Resolvers cache the record until TTL expires\n");
    printf("   • Multiple cache layers: Browser cache, OS cache, recursive resolver cache\n");
    printf("   • Negative caching: 'This domain doesn't exist' is also cached (RFC 2308)\n\n");
    printf("   Tradeoffs:\n");
    printf("   • Staleness: Changes to DNS records aren't seen until TTL expires.\n");
    printf("     If you change your server's IP, some users see old IP until cache expires.\n");
    printf("     Solution: Lower TTL before making changes (e.g., 24 hours before,\n");
    printf("     set TTL to 60 seconds)\n");
    printf("   • Memory: Caching requires RAM to store records\n");
    printf("   • Security: Cache poisoning attacks can inject fake records (DNSSEC helps)\n\n");
    printf("   Best practices:\n");
    printf("   • Static services: Use longer TTL (hours/days) for stability\n");
    printf("   • Services you might change: Use shorter TTL (minutes) for flexibility\n");
    printf("   • During migrations: Temporarily reduce TTL to 60-300 seconds\n");
    
    /* -------------- Part 6: Error handling --------------------------- */
    printf("\n=== Part 6: DNS Error Handling ===\n");
    printf("DNS queries can fail for many reasons:\n");
    printf("  - Domain doesn't exist (NXDOMAIN)\n");
    printf("  - Network timeout\n");
    printf("  - Invalid format\n");
    printf("Robust code must handle all error cases.\n");
    
    demonstrate_errors();
    
    wait_for_enter("Discuss: What errors should applications handle? Retry strategies?");
    
    printf("\n📖 ANSWER:\n");
    printf("   Common DNS errors (getaddrinfo return codes):\n");
    printf("   • EAI_NONAME: Domain doesn't exist (NXDOMAIN)\n");
    printf("     → Don't retry immediately; user likely mistyped\n");
    printf("   • EAI_AGAIN: Temporary failure (timeout, server busy)\n");
    printf("     → Safe to retry with exponential backoff\n");
    printf("   • EAI_FAIL: Non-recoverable failure (resolver configuration broken)\n");
    printf("     → Don't retry; log error and alert user\n");
    printf("   • EAI_MEMORY: Out of memory\n");
    printf("     → System-level issue; cleanup and retry or abort\n");
    printf("   • EAI_SYSTEM: Check errno for system error details\n\n");
    printf("   Retry strategies:\n");
    printf("   • Exponential backoff: 1s, 2s, 4s, 8s... (max ~30s)\n");
    printf("   • Jitter: Add randomness to avoid thundering herd\n");
    printf("   • Timeout: Set reasonable timeout (5-30s) to fail fast\n");
    printf("   • Circuit breaker: After N failures, stop trying for a cooling period\n\n");
    printf("   Production considerations:\n");
    printf("   • Fallback: If primary DNS fails, try secondary (most resolvers do this\n");
    printf("     automatically via /etc/resolv.conf)\n");
    printf("   • Health checks: Periodically verify DNS is working\n");
    printf("   • Monitoring: Alert on DNS failure rate spikes\n");
    printf("   • Graceful degradation: Use cached IPs or last-known-good config if DNS fails\n");
    
    /* ------------ Part 7: /etc/hosts vs DNS servers ------------------ */
    printf("\n=== Part 7: /etc/hosts vs DNS Server Resolution ===\n");
    
    demonstrate_hosts_file();
    
    wait_for_enter("Discuss: Resolution order? Security implications of /etc/hosts?");
    
    printf("\n📖 ANSWER:\n");
    printf("   Resolution order (typical Linux/Unix via /etc/nsswitch.conf):\n");
    printf("   1. files (/etc/hosts): Local static mappings\n");
    printf("   2. dns: Query DNS servers listed in /etc/resolv.conf\n");
    printf("   3. Alternative: Some systems support mDNS (Zeroconf), WINS, etc.\n\n");
    printf("   When /etc/hosts is useful:\n");
    printf("   • Local development: Map 'myapp.local' → 127.0.0.1 for testing\n");
    printf("   • Testing: Override production hostnames to point to test servers\n");
    printf("   • Performance: Skip DNS for frequently accessed local hosts\n");
    printf("   • Reliability: Critical services can have static entries as fallback\n");
    printf("   • Ad-blocking: Map ad domains to 0.0.0.0 (some ad blockers do this)\n\n");
    printf("   ⚠️  SECURITY IMPLICATIONS:\n");
    printf("   If an attacker modifies /etc/hosts, they can:\n");
    printf("   • Hijack traffic: Redirect 'bank.com' to attacker's server (phishing)\n");
    printf("   • Bypass security: Redirect security updates to malicious server\n");
    printf("   • Hide malware: Prevent antivirus from reaching update servers\n");
    printf("   • Poison environment: Redirect internal services\n\n");
    printf("   Protection:\n");
    printf("   • File permissions: /etc/hosts should be writable only by root (0644)\n");
    printf("   • File integrity: Monitor for unexpected changes (auditd, AIDE, Tripwire)\n");
    printf("   • Verification: On suspicious systems, check /etc/hosts manually\n\n");
    printf("   Best practices:\n");
    printf("   • Don't hardcode IPs in application code; let DNS work\n");
    printf("   • For production, use proper DNS instead of /etc/hosts\n");
    printf("   • Document any /etc/hosts entries; they're invisible to DNS audits\n");
    
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                    All sections complete!                    ║\n");
    printf("║                 Thanks for learning DNS!                     ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    
    return 0;
}

/* =======================================================================
                            DETAILED ANSWER KEY
   =======================================================================

Part 1 — Why is getaddrinfo preferred over gethostbyname?

    • Protocol independence: getaddrinfo supports both IPv4 and IPv6, while
      gethostbyname only supports IPv4. In modern networks with dual-stack
      configurations, IPv6 support is essential.
    
    • Thread safety: gethostbyname uses a static buffer for results, making it
      NOT thread-safe. Multiple threads calling it can overwrite each other's
      results. getaddrinfo allocates memory per-call (caller must free).
    
    • Modern API: getaddrinfo is the POSIX standard since 2001. gethostbyname
      is deprecated and may be removed in future systems.
    
    • Flexibility: getaddrinfo allows filtering by socket type, protocol, and
      can return results in ready-to-use sockaddr structures for socket calls.


Part 2 — What's the difference between A and AAAA records? Dual-stack?

    • A records: Return IPv4 addresses (32-bit, e.g., 142.250.185.78)
      Format: dotted decimal (4 octets)
    
    • AAAA records: Return IPv6 addresses (128-bit, e.g., 2607:f8b0:4004:c07::6a)
      Format: colon-hexadecimal (8 groups of 16 bits)
      Name: "AAAA" because IPv6 is 4 times larger than IPv4 (A)
    
    • Dual-stack: Systems that support both IPv4 and IPv6 simultaneously.
      When you use AF_UNSPEC with getaddrinfo, you get both A and AAAA records,
      allowing the application to choose which to use (Happy Eyeballs algorithm
      tries IPv6 first, falls back to IPv4 if IPv6 fails).
    
    • Why both?: IPv4 address exhaustion necessitates IPv6 migration, but IPv4
      remains ubiquitous, so most services support both for compatibility.


Part 3 — When is reverse DNS useful? Why might it fail?

    • Use cases:
      - Logging: Convert IP addresses in logs to readable hostnames
      - Email: SMTP servers check reverse DNS to verify sender legitimacy
      - Security: Verify that forward and reverse DNS match (FCrDNS check)
      - Troubleshooting: Identify what host an IP belongs to
    
    • Why it might fail:
      - PTR record not configured: Many hosts (especially clients) don't have
        reverse DNS configured. It's optional and requires ISP/admin setup.
      - Timeout: DNS server for the IP's reverse zone may be unreachable
      - Delegation: Reverse DNS requires proper delegation of in-addr.arpa or
        ip6.arpa zones, which may not be set up correctly
    
    • Security note: Reverse DNS can be controlled by whoever owns the IP block,
      so it's not cryptographically secure. Don't rely on it for authentication.


Part 4 — What are MX records used for? TXT records?

    • MX (Mail Exchange) records:
      - Purpose: Specify mail servers that accept email for a domain
      - Priority: Lower numbers = higher priority (try first)
      - Example: gmail.com → gmail-smtp-in.l.google.com (priority 5)
      - When you send email to user@example.com, your mail server queries
        MX records for example.com to find where to deliver the message
      - Multiple MX records provide redundancy and load balancing
    
    • TXT (Text) records:
      - Purpose: Store arbitrary text data, widely used for:
        * SPF (Sender Policy Framework): List IPs authorized to send email
          Example: "v=spf1 include:_spf.google.com ~all"
        * DKIM (DomainKeys Identified Mail): Public keys for email signing
        * Domain verification: Prove you own a domain (Google, Let's Encrypt)
        * DMARC: Email authentication policies
        * Human-readable information (now rare)
      - Originally for notes, now mostly machine-readable configuration
    
    • Other important record types:
      - NS: Delegate a subdomain to other nameservers
      - CNAME: Alias one name to another (canonical name)
      - SRV: Service location (port, weight, priority) for protocols


Part 5 — Why is DNS caching important? Tradeoffs?

    • Benefits of caching:
      - Performance: Avoid network round-trip for repeated queries. First lookup
        may take 20-100ms, cached lookups take <1ms
      - Scalability: Reduces load on authoritative DNS servers. Without caching,
        root and TLD servers would be overwhelmed
      - Reliability: If authoritative server is down, cached results still work
        until TTL expiry
      - Cost: DNS queries over network consume bandwidth and may have $ costs
    
    • How it works:
      - TTL (Time To Live): Each DNS record has a TTL (e.g., 300 seconds = 5 min)
        Resolvers cache the record until TTL expires
      - Multiple cache layers: Browser cache, OS cache, recursive resolver cache
      - Negative caching: "This domain doesn't exist" is also cached (RFC 2308)
    
    • Tradeoffs:
      - Staleness: Changes to DNS records aren't seen until TTL expires.
        If you change your server's IP, some users see the old IP until cache expires.
        Solution: Lower TTL before making changes (e.g., 24 hours before migration,
        set TTL to 60 seconds)
      - Memory: Caching requires RAM to store records
      - Security: Cache poisoning attacks can inject fake records
        (DNSSEC helps prevent this)
    
    • Best practices:
      - Static services: Use longer TTL (hours/days) for stability
      - Services you might change: Use shorter TTL (minutes) for flexibility
      - During migrations: Temporarily reduce TTL to 60-300 seconds


Part 6 — What errors should applications handle? Retry strategies?

    • Common DNS errors (getaddrinfo return codes):
      - EAI_NONAME: Domain doesn't exist (NXDOMAIN)
        → Don't retry immediately; user likely mistyped
      - EAI_AGAIN: Temporary failure (timeout, server busy)
        → Safe to retry with exponential backoff
      - EAI_FAIL: Non-recoverable failure (resolver configuration broken)
        → Don't retry; log error and alert user
      - EAI_MEMORY: Out of memory
        → System-level issue; cleanup and retry or abort
      - EAI_SYSTEM: Check errno for system error details
    
    • Retry strategies:
      - Exponential backoff: 1s, 2s, 4s, 8s... (max ~30s)
      - Jitter: Add randomness to avoid thundering herd
      - Timeout: Set reasonable timeout (5-30s) to fail fast
      - Circuit breaker: After N failures, stop trying for a cooling period
    
    • Production considerations:
      - Fallback: If primary DNS server fails, try secondary (most resolvers
        do this automatically via /etc/resolv.conf)
      - Health checks: Periodically verify DNS is working
      - Monitoring: Alert on DNS failure rate spikes
      - Graceful degradation: If DNS fails, can you use cached IPs or
        last-known-good configuration?
    
    • Security:
      - Validate input: Don't pass untrusted data directly to DNS queries
      - Limit rate: Prevent DNS amplification attacks
      - DNSSEC: Validate signatures when security is critical


Part 7 — Resolution order? Security implications of /etc/hosts?

    • Resolution order (typical Linux/Unix via /etc/nsswitch.conf):
      1. files (/etc/hosts): Local static mappings
      2. dns: Query DNS servers listed in /etc/resolv.conf
      3. Alternative: Some systems support mDNS (Zeroconf), WINS, etc.
    
    • /etc/hosts format:
        127.0.0.1       localhost
        ::1             localhost
        192.168.1.10    myserver.local myserver
      First column: IP address
      Remaining columns: Hostnames (first is "canonical")
    
    • When /etc/hosts is useful:
      - Local development: Map "myapp.local" → 127.0.0.1 for testing
      - Testing: Override production hostnames to point to test servers
      - Performance: Skip DNS for frequently accessed local hosts
      - Reliability: Critical services can have static entries as fallback
      - Ad-blocking: Map ad domains to 0.0.0.0 (some ad blockers do this)
    
    • Security implications:
      ⚠️ CRITICAL: /etc/hosts is read with root/admin privileges but checked
      for ALL users. If an attacker modifies /etc/hosts, they can:
      
      - Hijack traffic: Redirect "bank.com" to attacker's server (phishing)
      - Bypass security: Redirect security updates to malicious server
      - Hide malware: Prevent antivirus from reaching update servers
      - Poison environment: Redirect internal services
      
      Protection:
      - File permissions: /etc/hosts should be writable only by root (0644)
      - File integrity: Monitor for unexpected changes (auditd, AIDE, Tripwire)
      - Read-only root: Some systems make system files immutable
      - Verification: On suspicious systems, check /etc/hosts manually
    
    • /etc/resolv.conf (DNS server configuration):
        nameserver 8.8.8.8        # Google DNS
        nameserver 1.1.1.1        # Cloudflare DNS
        search example.com        # Append domain for short names
        options timeout:2         # Query timeout
      
      Modern systems often use systemd-resolved or NetworkManager to manage this.
    
    • Best practices:
      - Don't hardcode IPs in application code; let DNS work
      - For production, use proper DNS instead of /etc/hosts
      - Document any /etc/hosts entries; they're invisible to DNS audits
      - In containers: Each container can have its own /etc/hosts (useful for
        service discovery without DNS infrastructure)

=======================================================================
                     ADDITIONAL LEARNING TOPICS
=======================================================================

Advanced topics to explore:

1. DNS Security:
   - DNSSEC: Cryptographic signatures to prevent cache poisoning
   - DNS over HTTPS (DoH): Encrypt DNS queries for privacy
   - DNS over TLS (DoT): Similar to DoH but different protocol

2. Performance optimization:
   - Happy Eyeballs (RFC 8305): Try IPv6 and IPv4 in parallel
   - Prefetching: Resolve hostnames before they're needed
   - Connection pooling: Reuse connections to same host

3. Load balancing via DNS:
   - Round-robin DNS: Multiple A records, different order per query
   - GeoDNS: Return different IPs based on query source location
   - Weighted records: Distribute traffic by percentage

4. DNS in distributed systems:
   - Service discovery: Consul, etcd, Kubernetes DNS
   - Global server load balancing (GSLB)
   - Anycast: Same IP announced from multiple locations

5. Troubleshooting tools:
   - dig: Query DNS directly, see full responses
   - nslookup: Interactive DNS query tool
   - host: Simple DNS lookup
   - tcpdump/wireshark: Inspect DNS packets on wire

Example dig commands:
  dig www.google.com A          # Get IPv4 address
  dig www.google.com AAAA       # Get IPv6 address
  dig google.com MX             # Get mail servers
  dig google.com NS             # Get nameservers
  dig @8.8.8.8 google.com       # Query specific DNS server
  dig +trace google.com         # Show full resolution path

=======================================================================
*/

