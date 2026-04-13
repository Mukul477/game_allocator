#include <stdio.h>
#include <math.h>
#include "predictor.h"

#define NUM_WEIGHTS    5
#define LEARNING_RATE  0.0001f
#define MAX_FRAMES     3000.0f
#define MAX_SIZE       1024.0f
#define MAX_LIFETIME   500.0f

static float weights[NUM_WEIGHTS];
static float spawn_rate[3];

/* Track MSE in buckets of 1000 updates each */
#define NUM_BUCKETS 30
static float bucket_sum[NUM_BUCKETS];
static int   bucket_count[NUM_BUCKETS];

static int update_count   = 0;
static int correct_routes = 0;
static int total_routes   = 0;

/* Per-type prediction accuracy */
static float type_error_sum[3];
static int   type_error_count[3];

static Features build_features(ObjectType type, float size, int frame)
{
    Features f;
    f.obj_type     = (float)type / 2.0f;
    f.size         = size / MAX_SIZE;
    f.frame_number = (float)frame / MAX_FRAMES;
    f.spawn_rate   = spawn_rate[type] / 10.0f;
    return f;
}

static float model_predict(const Features* f)
{
    float p = weights[0]
            + weights[1] * f->obj_type
            + weights[2] * f->size
            + weights[3] * f->frame_number
            + weights[4] * f->spawn_rate;
    return p < 1.0f ? 1.0f : p;
}

void predictor_init(void)
{
    weights[0] =  3.0f;
    weights[1] = 99.8f;
    weights[2] =  0.0f;
    weights[3] =  0.0f;
    weights[4] = -1.0f;

    for (int i = 0; i < 3; i++) {
        spawn_rate[i]      = 0.0f;
        type_error_sum[i]  = 0.0f;
        type_error_count[i]= 0;
    }
    for (int i = 0; i < NUM_BUCKETS; i++) {
        bucket_sum[i]   = 0.0f;
        bucket_count[i] = 0;
    }

    update_count   = 0;
    correct_routes = 0;
    total_routes   = 0;
}

float predict_lifetime(ObjectType type, float size, int frame)
{
    spawn_rate[type] = 0.9f * spawn_rate[type] + 1.0f;
    for (int i = 0; i < 3; i++)
        if (i != (int)type) spawn_rate[i] *= 0.95f;

    Features f    = build_features(type, size, frame);
    float    pred = model_predict(&f);

    total_routes++;
    int predicted_short = (pred < SHORT_LIVED_THRESHOLD);
    int actually_short  = (type == PARTICLE);
    if (predicted_short == actually_short) correct_routes++;

    return pred;
}

void update_weights(ObjectType type, float size, int alloc_frame,
                    int free_frame)
{
    float actual = (float)(free_frame - alloc_frame);
    if (actual < 1.0f)         actual = 1.0f;
    if (actual > MAX_LIFETIME) actual = MAX_LIFETIME;

    Features f    = build_features(type, size, alloc_frame);
    float    pred = model_predict(&f);
    float    error = actual - pred;

    weights[0] += LEARNING_RATE * error;
    weights[1] += LEARNING_RATE * error * f.obj_type;
    weights[2] += LEARNING_RATE * error * f.size;
    weights[3] += LEARNING_RATE * error * f.frame_number;
    weights[4] += LEARNING_RATE * error * f.spawn_rate;

    /* Bucket MSE — each bucket = 1000 updates */
    int bucket = update_count / 1000;
    if (bucket < NUM_BUCKETS) {
        bucket_sum[bucket]   += error * error;
        bucket_count[bucket] += 1;
    }

    /* Per-type error */
    if ((int)type < 3) {
        type_error_sum  [(int)type] += fabsf(error);
        type_error_count[(int)type] += 1;
    }

    update_count++;
}

void predictor_print_stats(void)
{
    printf("\n========================================\n");
    printf("  PREDICTOR  (Online Linear Regression)\n");
    printf("========================================\n");
    printf("  Training samples : %d\n\n", update_count);

    printf("  Weights learned:\n");
    printf("    w0  bias        = %8.3f\n", weights[0]);
    printf("    w1  obj_type    = %8.3f  (dominant — type drives lifetime)\n", weights[1]);
    printf("    w2  size        = %8.3f\n", weights[2]);
    printf("    w3  frame_no    = %8.3f\n", weights[3]);
    printf("    w4  spawn_rate  = %8.3f\n", weights[4]);

    /* MSE learning curve across buckets */
    printf("\n  MSE learning curve (per 1000 updates):\n");
    int printed = 0;
    for (int i = 0; i < NUM_BUCKETS; i++) {
        if (bucket_count[i] == 0) break;
        float mse = bucket_sum[i] / (float)bucket_count[i];
        int bar = (int)(mse / 500.0f);
        if (bar > 40) bar = 40;
        printf("    updates %5d-%5d | MSE=%8.1f |",
               i * 1000, i * 1000 + bucket_count[i] - 1, mse);
        for (int b = 0; b < bar; b++) printf("#");
        printf("\n");
        printed++;
    }

    /* Per-type mean absolute error */
    printf("\n  Mean absolute error per object type:\n");
    const char* names[3] = {"Particle (true=3  )", "Bullet   (true=50 )", "Enemy    (true=500)"};
    for (int i = 0; i < 3; i++) {
        if (type_error_count[i] > 0) {
            float mae = type_error_sum[i] / (float)type_error_count[i];
            printf("    %-26s MAE = %.2f frames\n", names[i], mae);
        }
    }

    float acc = total_routes > 0
                ? (float)correct_routes / (float)total_routes * 100.0f
                : 0.0f;
    printf("\n  Routing accuracy : %d / %d  (%.1f%%)\n",
           correct_routes, total_routes, acc);
    printf("  Rule: predicted < %.0f frames → slab  (short-lived)\n", SHORT_LIVED_THRESHOLD);
    printf("        predicted >=%.0f frames → pool  (long-lived)\n",  SHORT_LIVED_THRESHOLD);
    printf("========================================\n");
}