#include "msutil.h"
#include <errno.h>
#include <fcntl.h> // open(2)
#include "tap.h"
#include "acism.h"

#ifdef ACISM_STATS
typedef struct { long long val; const char *name; } PSSTAT;
extern PSSTAT psstat[];
extern int pscand[];
#endif//ACISM_STATS

static FILE *matchfp;
static int actual = 0;
static MEMREF *pattv;
static int text_matchs = 0;
static int shortest_strnum = 0;

static int
on_match(int strnum, int textpos, MEMREF *text)
{
    (void)strnum, (void)textpos, (void)text;

    int collision = 0;

    if  ( (int)text[0].len < (int)pattv[strnum].len )
      collision = 1;
    else
      collision = !strncmp(text[0].ptr + (int)text[0].len - (int)pattv[strnum].len, pattv[strnum].ptr, (int)pattv[strnum].len) ? 0 : 1;

    text_matchs++;

    if ( !collision ) {
      if ( text_matchs == 1 )
        shortest_strnum = strnum;
      else if ( ((int)pattv[shortest_strnum].len) >= ((int)pattv[strnum].len) ) // We are already matching the same text
        shortest_strnum = strnum;
    }


  if (matchfp) fprintf(matchfp, "%9d %7d %d %d %d %.*s %.*s\n", textpos, strnum, collision, text_matchs, shortest_strnum, (int)pattv[strnum].len, pattv[strnum].ptr, (int)text[0].len, text[0].ptr);

  ++actual;
  return 0;
}

int
main(int argc, char **argv)
{
    if (argc < 2 || argc > 4)
        usage("pattern_file [nmatches [match.log]]\n" "Creates acism.tmp");

    MEMBUF patt = chomp(slurp(argv[1]));
    if (!patt.ptr)
        die("cannot read %s", argv[1]);

    int npatts;
    pattv = refsplit(patt.ptr, '\n', &npatts);

    double t = tick();
    ACISM *psp = acism_create(pattv, npatts);
    t = tick() - t;

    plan_tests(argc < 3 ? 1 : 3);

    ok(psp, "acism_create(pattv[%d]) compiled, in %.3f secs", npatts, t);
    acism_dump(psp, PS_STATS, stderr, pattv);
#ifdef ACISM_STATS
    {
    int i;
    for (i = 1; i < (int)psstat[0].val; ++i)
        if (psstat[i].val)
            fprintf(stderr, "%11llu %s\n", psstat[i].val, psstat[i].name);
    }
#endif//ACISM_STATS

    diag("state machine saved as acism.tmp");
    FILE *fp = fopen("acism.tmp", "w");
    acism_save(fp, psp);
    fclose(fp);

    if (argc > 2) {

        int npattsin;
        MEMBUF pattin = chomp(slurp(argv[2]));

        if (!pattin.ptr)
            die("cannot read %s", argv[2]);

        double elapsed = 0, start = tick();
        MEMREF *pattvin = refsplit(pattin.ptr, '\n', &npattsin);
        fprintf(stderr, "in patterns: %d\n",npattsin);
        if (argc > 3) matchfp = fopen(argv[3], "w");

        FILE *shortest = fopen("shortest.log", "w");
        int i;
	      for (i =0; i < npattsin; i++) {
           t = tick();
           text_matchs = 0;
           (void)acism_scan(psp, pattvin[i], (ACISM_ACTION*)on_match, &pattvin[i]);
           elapsed += tick() - t;
           fprintf(shortest, "%.*s\n", (int)pattv[shortest_strnum].len, pattv[shortest_strnum].ptr);
        }
        putc('\n', stderr);
        buffree(pattin);
        free(pattvin);
        fclose(shortest);
        if (matchfp) fclose(matchfp);
        ok(1,"pattern file scanned; read(s) took %.3f secs", tick() - start - elapsed);

        int expected = argc > 2 ? atoi(argv[2]) : actual;
        if (!ok(actual == expected, "%d matches found, in %.3f secs", expected, elapsed))
            diag("actual: %d\n", actual);
    }

    buffree(patt);
    free(pattv);
    acism_destroy(psp);

    return exit_status();
}
