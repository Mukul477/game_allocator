#ifndef PREDICTOR_H
#define PREDICTOR_H
 
#include "allocator.h"
 
typedef struct {
    float obj_type;     
    float size;        
    float frame_number; 
    float spawn_rate;  
} Features;
 
void predictor_init(void);
 
float predict_lifetime(ObjectType type, float size, int frame);
 
void update_weights(ObjectType type, float size, int alloc_frame,
                    int free_frame);

void predictor_print_stats(void);

#define SHORT_LIVED_THRESHOLD 10.0f
 
#endif