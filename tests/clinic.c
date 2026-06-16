/* clinic.c - "object-oriented C" patient-registration stress test for dcc.
 *
 * Standalone (not wired into runall); build/run with:
 *     ./ma.sh clinic peep   &&  ntvcm CLINIC
 *     ./ma.sh clinic nopeep &&  ntvcm CLINIC
 *
 * This pushes the dcc code generator on deeply nested aggregate access where
 * EVERY value at EVERY level is a closed-form function of its indices, so the
 * whole data set can be re-derived and checked element by element:
 *
 *   Clinic
 *     +-- Department depts[NDEPT]          (array of structs embedded in struct)
 *           +-- const BillingVTable *billing   (per-department vtable / policy)
 *           +-- Patient patients[NPAT]      (array of structs embedded in struct)
 *                 +-- char *name           (malloc'd string in a struct array)
 *                 +-- const CategoryVTable *vt  (per-element vtable)
 *                 +-- Visit *visits        (malloc'd array hung off a struct array)
 *                       +-- int *labs      (malloc'd array hung off a malloc'd array)
 *
 * The index patterns (gid = dept*NPAT + patient):
 *     age            = 40 + gid                 (40, 41, 42, ... -- the "age" base)
 *     name           = "P%02d" % gid            ("P00", "P01", ...)
 *     nvisits        = patient + 1
 *     visit.day      = gid*10 + v
 *     visit.systolic = 100 + gid + v
 *     visit.diastolic= 60 + v
 *     visit.cost     = 200 + gid*10 + v*5
 *     nlabs          = v + 1
 *     lab[l]         = gid*100 + v*10 + l
 *
 * Two vtables provide the OO polymorphism:
 *   - BillingVTable (per department): SelfPay / HMO / PPO compute the patient's
 *     charge from a visit's base cost in three different ways.
 *   - CategoryVTable (per patient, chosen by age bracket): Standard / Priority /
 *     Senior compute a "risk score" differently.
 *
 * Verification walks the live structure, re-derives the expected value from the
 * indices at every level, and also checks a handful of absolute aggregate
 * anchors (total patients/visits/labs and the age/risk/billing sums).  Finishes
 * with "clinic passed with great success" or a non-zero exit + a FAIL line.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NDEPT 3
#define NPAT  4     /* patients per department */

/* ------------------------------------------------------------------ */
/* self-checking harness                                               */
/* ------------------------------------------------------------------ */

static int g_checks;
static int g_fails;

static void check_l(const char *name, long got, long expected)
{
    g_checks++;
    if (got != expected) {
        g_fails++;
        printf("  FAIL %s: got %ld expected %ld\n", name, got, expected);
    }
}

static void check_i(const char *name, int got, int expected)
{
    check_l(name, (long)got, (long)expected);
}

static void check_s(const char *name, const char *got, const char *expected)
{
    g_checks++;
    if (strcmp(got, expected) != 0) {
        g_fails++;
        printf("  FAIL %s: got \"%s\" expected \"%s\"\n", name, got, expected);
    }
}

static void *xmalloc(unsigned int n)
{
    void *p = malloc(n);
    if (p == 0) {
        printf("  FAIL out of memory (%u bytes)\n", n);
        exit(2);
    }
    return p;
}

/* ------------------------------------------------------------------ */
/* leaf records                                                        */
/* ------------------------------------------------------------------ */

typedef struct Visit {
    int  day;
    int  systolic;
    int  diastolic;
    long cost;          /* base cost in whole dollars */
    int  nlabs;
    int *labs;          /* malloc'd array of lab values */
} Visit;

struct Patient;         /* forward declaration for the vtable signatures */

/* ---- per-patient category vtable (chosen by age bracket) ---- */

typedef int (*risk_fn)(const struct Patient *p);

typedef struct CategoryVTable {
    const char *name;
    risk_fn     risk;
} CategoryVTable;

typedef struct Patient {
    char *name;                     /* malloc'd "P%02d" */
    int   age;
    int   gid;
    int   nvisits;
    Visit *visits;                  /* malloc'd array of Visit */
    const CategoryVTable *vt;       /* virtual category dispatch */
} Patient;

/* ---- per-department billing vtable (policy) ---- */

typedef long (*charge_fn)(const Visit *v);

typedef struct BillingVTable {
    const char *name;
    charge_fn   charge;
} BillingVTable;

typedef struct Department {
    char *name;                     /* malloc'd "DPT%d" */
    const BillingVTable *billing;   /* virtual billing policy */
    int   npatients;
    Patient patients[NPAT];         /* array of structs embedded in a struct */
} Department;

typedef struct Clinic {
    const char *name;
    int ndepts;
    Department depts[NDEPT];        /* array of structs embedded in a struct */
} Clinic;

/* ------------------------------------------------------------------ */
/* category vtable implementations (risk score)                        */
/* ------------------------------------------------------------------ */

static int risk_standard(const Patient *p) { return p->age; }
static int risk_priority(const Patient *p) { return p->age + p->nvisits * 2; }
static int risk_senior(const Patient *p)   { return p->age * 2 + p->nvisits; }

static const CategoryVTable CAT_STANDARD = { "Standard", risk_standard };
static const CategoryVTable CAT_PRIORITY = { "Priority", risk_priority };
static const CategoryVTable CAT_SENIOR   = { "Senior",   risk_senior };

/* age brackets: <45 Standard, 45..49 Priority, >=50 Senior */
static const CategoryVTable *category_for_age(int age)
{
    if (age < 45)
        return &CAT_STANDARD;
    if (age < 50)
        return &CAT_PRIORITY;
    return &CAT_SENIOR;
}

/* ------------------------------------------------------------------ */
/* billing vtable implementations (charge)                             */
/* ------------------------------------------------------------------ */

static long charge_selfpay(const Visit *v) { return v->cost; }
static long charge_hmo(const Visit *v)     { (void)v; return 20L; }
static long charge_ppo(const Visit *v)     { return v->cost / 5L; }

static const BillingVTable BILL_SELFPAY = { "SelfPay", charge_selfpay };
static const BillingVTable BILL_HMO     = { "HMO",     charge_hmo };
static const BillingVTable BILL_PPO     = { "PPO",     charge_ppo };

static const BillingVTable *POLICIES[NDEPT] = {
    &BILL_SELFPAY, &BILL_HMO, &BILL_PPO
};

/* generic dispatchers - the only code that "knows" the vtable layout */
static long visit_charge(const Department *d, const Visit *v)
{
    return d->billing->charge(v);
}

static int patient_risk(const Patient *p)
{
    return p->vt->risk(p);
}

/* ------------------------------------------------------------------ */
/* construction - everything is a function of the indices              */
/* ------------------------------------------------------------------ */

static void patient_init(Patient *pt, int d, int p)
{
    int gid = d * NPAT + p;
    int nvisits = p + 1;
    int v, l;
    char tmp[8];

    pt->gid = gid;
    pt->age = 40 + gid;
    pt->nvisits = nvisits;

    sprintf(tmp, "P%02d", gid);
    pt->name = (char *)xmalloc((unsigned int)(strlen(tmp) + 1));
    strcpy(pt->name, tmp);

    pt->vt = category_for_age(pt->age);

    pt->visits = (Visit *)xmalloc((unsigned int)(nvisits * (int)sizeof(Visit)));
    for (v = 0; v < nvisits; v++) {
        Visit *vs = &pt->visits[v];
        int nlabs = v + 1;

        vs->day = gid * 10 + v;
        vs->systolic = 100 + gid + v;
        vs->diastolic = 60 + v;
        vs->cost = 200L + gid * 10 + v * 5;
        vs->nlabs = nlabs;
        vs->labs = (int *)xmalloc((unsigned int)(nlabs * (int)sizeof(int)));
        for (l = 0; l < nlabs; l++)
            vs->labs[l] = gid * 100 + v * 10 + l;
    }
}

static void dept_init(Department *dp, int d)
{
    int p;
    char tmp[8];

    sprintf(tmp, "DPT%d", d);
    dp->name = (char *)xmalloc((unsigned int)(strlen(tmp) + 1));
    strcpy(dp->name, tmp);

    dp->billing = POLICIES[d % NDEPT];
    dp->npatients = NPAT;

    for (p = 0; p < NPAT; p++)
        patient_init(&dp->patients[p], d, p);
}

static void clinic_init(Clinic *cl)
{
    int d;

    cl->name = "Mercy Clinic";
    cl->ndepts = NDEPT;
    for (d = 0; d < NDEPT; d++)
        dept_init(&cl->depts[d], d);
}

static void clinic_free(Clinic *cl)
{
    int d, p, v;

    for (d = 0; d < cl->ndepts; d++) {
        Department *dp = &cl->depts[d];
        for (p = 0; p < dp->npatients; p++) {
            Patient *pt = &dp->patients[p];
            for (v = 0; v < pt->nvisits; v++)
                free(pt->visits[v].labs);   /* deepest malloc first */
            free(pt->visits);
            free(pt->name);
        }
        free(dp->name);
    }
}

/* ------------------------------------------------------------------ */
/* the clinic lives in static storage (large, embedded arrays)         */
/* ------------------------------------------------------------------ */

static Clinic g_clinic;

/* ------------------------------------------------------------------ */
/* verification: re-derive every value from its indices                */
/* ------------------------------------------------------------------ */

static void test_structure(void)
{
    int d, p, v, l;
    int n_patients = 0, n_visits = 0, n_labs = 0;
    long age_sum = 0;
    char tmp[8];

    printf("structure:\n");

    for (d = 0; d < g_clinic.ndepts; d++) {
        Department *dp = &g_clinic.depts[d];

        sprintf(tmp, "DPT%d", d);
        check_s("dept.name", dp->name, tmp);
        check_i("dept.npatients", dp->npatients, NPAT);

        for (p = 0; p < dp->npatients; p++) {
            Patient *pt = &dp->patients[p];
            int gid = d * NPAT + p;

            check_i("pt.gid", pt->gid, gid);
            check_i("pt.age", pt->age, 40 + gid);
            check_i("pt.nvisits", pt->nvisits, p + 1);

            sprintf(tmp, "P%02d", gid);
            check_s("pt.name", pt->name, tmp);

            n_patients++;
            age_sum += pt->age;

            for (v = 0; v < pt->nvisits; v++) {
                Visit *vs = &pt->visits[v];

                check_i("visit.day", vs->day, gid * 10 + v);
                check_i("visit.systolic", vs->systolic, 100 + gid + v);
                check_i("visit.diastolic", vs->diastolic, 60 + v);
                check_l("visit.cost", vs->cost, 200L + gid * 10 + v * 5);
                check_i("visit.nlabs", vs->nlabs, v + 1);

                n_visits++;

                for (l = 0; l < vs->nlabs; l++) {
                    check_i("lab", vs->labs[l], gid * 100 + v * 10 + l);
                    n_labs++;
                }
            }
        }
    }

    /* absolute aggregate anchors (guard against miscount / skipped nodes) */
    check_i("count.patients", n_patients, NDEPT * NPAT);   /* 12 */
    check_i("count.visits", n_visits, 30);
    check_i("count.labs", n_labs, 60);
    check_l("sum.age", age_sum, 546L);

    printf("  %d patients, %d visits, %d labs; age sum=%ld\n",
           n_patients, n_visits, n_labs, age_sum);
}

static void test_billing(void)
{
    int d, p, v;
    long grand = 0;
    long dept_totals[NDEPT];

    printf("billing:\n");

    for (d = 0; d < g_clinic.ndepts; d++) {
        Department *dp = &g_clinic.depts[d];
        int policy = d % NDEPT;
        long dtotal = 0;

        for (p = 0; p < dp->npatients; p++) {
            Patient *pt = &dp->patients[p];
            for (v = 0; v < pt->nvisits; v++) {
                Visit *vs = &pt->visits[v];
                long via_vt = visit_charge(dp, vs);  /* polymorphic dispatch */
                long direct;

                /* independent recomputation by policy index */
                if (policy == 0)
                    direct = vs->cost;
                else if (policy == 1)
                    direct = 20L;
                else
                    direct = vs->cost / 5L;

                check_l("charge.dispatch", via_vt, direct);
                dtotal += via_vt;
            }
        }
        dept_totals[d] = dtotal;
        grand += dtotal;
        printf("  %-3s %-7s total=%ld\n",
               dp->name, dp->billing->name, dtotal);
    }

    /* anchors derived from the index patterns */
    check_l("bill.dept0", dept_totals[0], 2250L);   /* SelfPay, full cost */
    check_l("bill.dept1", dept_totals[1], 200L);    /* HMO, flat 20 x 10 visits */
    check_l("bill.dept2", dept_totals[2], 610L);    /* PPO, cost/5 */
    check_l("bill.grand", grand, 3060L);
}

static void test_risk(void)
{
    int d, p;
    long risk_sum = 0;

    printf("risk:\n");

    for (d = 0; d < g_clinic.ndepts; d++) {
        Department *dp = &g_clinic.depts[d];
        for (p = 0; p < dp->npatients; p++) {
            Patient *pt = &dp->patients[p];
            int via_vt = patient_risk(pt);          /* polymorphic dispatch */
            const CategoryVTable *exp_cat = category_for_age(pt->age);
            int direct;

            /* expected category name and risk recomputed from the bracket */
            check_s("risk.category", pt->vt->name, exp_cat->name);

            if (pt->age < 45)
                direct = pt->age;                   /* Standard */
            else if (pt->age < 50)
                direct = pt->age + pt->nvisits * 2; /* Priority */
            else
                direct = pt->age * 2 + pt->nvisits; /* Senior */

            check_i("risk.dispatch", via_vt, direct);
            risk_sum += via_vt;
        }
    }

    check_l("risk.sum", risk_sum, 678L);
    printf("  risk sum=%ld\n", risk_sum);
}

static void test_pointer_index(void)
{
    Patient *all[NDEPT * NPAT];
    int d, p, i, count = 0;
    Patient **pp;
    Patient **end;
    Patient *deep;

    printf("pointer index:\n");

    /* build a flat index of base pointers into the embedded struct arrays */
    for (d = 0; d < g_clinic.ndepts; d++)
        for (p = 0; p < g_clinic.depts[d].npatients; p++)
            all[count++] = &g_clinic.depts[d].patients[p];

    check_i("index.count", count, NDEPT * NPAT);

    /* pointer-to-pointer walk; gid runs 0..11 in fill order */
    i = 0;
    pp = all;
    end = all + count;
    while (pp < end) {
        check_i("index.gid", (*pp)->gid, i);
        check_i("index.age", (*pp)->age, 40 + i);
        pp++;
        i++;
    }

    /* pointer identity: the indexed pointer and the structure path alias */
    g_checks++;
    if (all[5] != &g_clinic.depts[1].patients[1]) {
        g_fails++;
        printf("  FAIL index alias mismatch\n");
    }

    /* mutate through the flat pointer, observe through the nested path */
    all[7]->age = 999;
    check_i("alias.write", g_clinic.depts[1].patients[3].age, 999);
    all[7]->age = 40 + 7;   /* restore */
    check_i("alias.restore", g_clinic.depts[1].patients[3].age, 47);

    /* deepest chain: embedded dept -> embedded patient -> malloc'd visit ->
       malloc'd lab.  gid=11, last visit v=3, last lab l=3 => 1100+30+3 */
    deep = &g_clinic.depts[2].patients[3];
    check_i("deep.gid", deep->gid, 11);
    check_i("deep.lab", deep->visits[3].labs[3], 1133);

    printf("  %d patients indexed; deepest lab=%d\n",
           count, deep->visits[3].labs[3]);
}

int main(void)
{
    printf("clinic: OO-style patient registration stress test for dcc\n");

    clinic_init(&g_clinic);

    test_structure();
    test_billing();
    test_risk();
    test_pointer_index();

    clinic_free(&g_clinic);

    printf("ran %d checks, %d failures\n", g_checks, g_fails);
    if (g_fails == 0) {
        printf("clinic passed with great success\n");
        return 0;
    }
    printf("clinic FAILED\n");
    return 1;
}
