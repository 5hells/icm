#include "ipc_server.h"
#include "ipc_protocol.h"
#include "transform_matrix.h"
#include "main.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <errno.h>
#include <math.h>
#include <ctype.h>
#include <stdbool.h>
#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wayland-server-protocol.h>
#include <wlr/util/log.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <pango/pangocairo.h>
#include <cairo.h>

/* Forward declarations */
void draw_window_decorations(struct BufferEntry *buffer);
void start_buffer_animation(struct BufferEntry *buffer, uint32_t duration_ms);
void build_transform_matrix(float *matrix, float tx, float ty, float tz,
                           float rx, float ry, float rz, float sx, float sy, float sz);
void update_buffer_animation(struct BufferEntry *buffer, uint32_t current_time);
void update_animations(struct IPCServer *ipc_server);
static int handle_set_window_matrix(struct IPCServer *ipc_server, struct IPCClient *client,
                                    const struct icm_msg_set_window_matrix *msg);

/* Socket I/O helpers */
static ssize_t send_with_fds(int socket_fd, const void *data, size_t size,
                             const int *fds, int num_fds) {
    if (num_fds == 0) {
        return send(socket_fd, data, size, 0);
    }

    struct cmsghdr *cmsg;
    struct msghdr msg = {0};
    struct iovec iov = {
        .iov_base = (void *)data,
        .iov_len = size,
    };

    char cmsgbuf[CMSG_SPACE(num_fds * sizeof(int))];
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(num_fds * sizeof(int));
    memcpy(CMSG_DATA(cmsg), fds, num_fds * sizeof(int));

    msg.msg_controllen = cmsg->cmsg_len;

    return sendmsg(socket_fd, &msg, 0);
}

/* Simple expression evaluator for pixel effects */
#include <math.h>
#include <ctype.h>

typedef struct {
    double r, g, b, a;
} PixelVars;

static double clamp(double val, double min, double max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

/*
    equation format:
    deff blur_radius 5.0
    defn blur_sample(dx, dy) {
        int sample_x = clamp(x + dx, 0, width - 1);
        int sample_y = clamp(y + dy, 0, height - 1);
        int idx = (sample_y * width + sample_x) * 4;
        return [pixels[idx], pixels[idx + 1], pixels[idx + 2], pixels[idx + 3]];
    }
    defn blur(chunk4) {
        chunk4 result = [0, 0, 0, 0];
        int count = 0;
        for (int dx = -blur_radius; dx <= blur_radius; dx++) {
            for (int dy = -blur_radius; dy <= blur_radius; dy++) {
                chunk4 sample = blur_sample(dx, dy);
                result[0] += sample[0];
                result[1] += sample[1];
                result[2] += sample[2];
                result[3] += sample[3];
                count++;
            }
        }
        result[0] /= count;
        result[1] /= count;
        result[2] /= count;
        result[3] /= count;
        return result;
    }
    chunk4*:[r, g, b, a] = blur([r, g, b, a]);
*/

/* Advanced pixel effect interpreter */

#define MAX_VARS 256
#define MAX_FUNCTIONS 64
#define MAX_ARRAY_SIZE 16
#define MAX_STACK_DEPTH 32

typedef enum {
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_ARRAY
} ValueType;

typedef struct {
    ValueType type;
    union {
        int i;
        float f;
        float *arr; // Change fixed-size array to a pointer
    } value;
    int array_size;
} Value;

typedef struct {
    char name[32];
    Value value;
} Variable;

typedef struct {
    char name[32];
    char *body;
    char params[128];
} Function;

typedef struct {
    Variable vars[MAX_VARS];
    int var_count;
    Function functions[MAX_FUNCTIONS];
    int func_count;
    uint8_t *pixels;
    size_t width, height;
    size_t current_x, current_y;
    double time_seconds;
} Interpreter;

static Interpreter *interpreter_create(uint8_t *pixels, size_t width, size_t height,
		double time_seconds) {
    Interpreter *interp = calloc(1, sizeof(Interpreter));
    interp->pixels = pixels;
    interp->width = width;
    interp->height = height;
    interp->time_seconds = time_seconds;
    
    // Add built-in functions
    strcpy(interp->functions[interp->func_count].name, "clamp");
    interp->functions[interp->func_count].body = strdup("return min(max(val, min_val), max_val);");
    strcpy(interp->functions[interp->func_count].params, "val, min_val, max_val");
    interp->func_count++;
    
    strcpy(interp->functions[interp->func_count].name, "min");
    interp->functions[interp->func_count].body = strdup("return a < b ? a : b;");
    strcpy(interp->functions[interp->func_count].params, "a, b");
    interp->func_count++;
    
    strcpy(interp->functions[interp->func_count].name, "max");
    interp->functions[interp->func_count].body = strdup("return a > b ? a : b;");
    strcpy(interp->functions[interp->func_count].params, "a, b");
    interp->func_count++;
    
    return interp;
}

static void interpreter_destroy(Interpreter *interp) {
    for (int i = 0; i < interp->func_count; i++) {
        free(interp->functions[i].body);
    }
    free(interp);
}

static Variable *find_var(Interpreter *interp, const char *name) {
    for (int i = 0; i < interp->var_count; i++) {
        if (strcmp(interp->vars[i].name, name) == 0) {
            return &interp->vars[i];
        }
    }
    return NULL;
}

static Function *find_func(Interpreter *interp, const char *name) {
    for (int i = 0; i < interp->func_count; i++) {
        if (strcmp(interp->functions[i].name, name) == 0) {
            return &interp->functions[i];
        }
    }
    return NULL;
}

// Forward declarations
static void set_var(Interpreter *interp, const char *name, Value value);
static Value get_var(Interpreter *interp, const char *name);
static Value evaluate_expression(Interpreter *interp, const char *expr);

static Value call_function(Interpreter *interp, const char *name, Value *args, int arg_count) {
    if (strcmp(name, "sin") == 0 && arg_count >= 1) {
        float v = args[0].type == TYPE_FLOAT ? args[0].value.f : args[0].value.i;
        return (Value){TYPE_FLOAT, .value.f = sinf(v)};
    }
    if (strcmp(name, "cos") == 0 && arg_count >= 1) {
        float v = args[0].type == TYPE_FLOAT ? args[0].value.f : args[0].value.i;
        return (Value){TYPE_FLOAT, .value.f = cosf(v)};
    }
    if (strcmp(name, "tan") == 0 && arg_count >= 1) {
        float v = args[0].type == TYPE_FLOAT ? args[0].value.f : args[0].value.i;
        return (Value){TYPE_FLOAT, .value.f = tanf(v)};
    }
    if (strcmp(name, "pow") == 0 && arg_count >= 2) {
        float a = args[0].type == TYPE_FLOAT ? args[0].value.f : args[0].value.i;
        float b = args[1].type == TYPE_FLOAT ? args[1].value.f : args[1].value.i;
        return (Value){TYPE_FLOAT, .value.f = powf(a, b)};
    }
    if (strcmp(name, "sqrt") == 0 && arg_count >= 1) {
        float v = args[0].type == TYPE_FLOAT ? args[0].value.f : args[0].value.i;
        return (Value){TYPE_FLOAT, .value.f = sqrtf(fmaxf(v, 0.0f))};
    }
    if (strcmp(name, "abs") == 0 && arg_count >= 1) {
        if (args[0].type == TYPE_FLOAT) {
            return (Value){TYPE_FLOAT, .value.f = fabsf(args[0].value.f)};
        }
        return (Value){TYPE_INT, .value.i = abs(args[0].value.i)};
    }
    if (strcmp(name, "floor") == 0 && arg_count >= 1) {
        float v = args[0].type == TYPE_FLOAT ? args[0].value.f : args[0].value.i;
        return (Value){TYPE_FLOAT, .value.f = floorf(v)};
    }
    if (strcmp(name, "ceil") == 0 && arg_count >= 1) {
        float v = args[0].type == TYPE_FLOAT ? args[0].value.f : args[0].value.i;
        return (Value){TYPE_FLOAT, .value.f = ceilf(v)};
    }
    if (strcmp(name, "fract") == 0 && arg_count >= 1) {
        float v = args[0].type == TYPE_FLOAT ? args[0].value.f : args[0].value.i;
        float f = v - floorf(v);
        return (Value){TYPE_FLOAT, .value.f = f};
    }
    if (strcmp(name, "mix") == 0 && arg_count >= 3) {
        float a = args[0].type == TYPE_FLOAT ? args[0].value.f : args[0].value.i;
        float b = args[1].type == TYPE_FLOAT ? args[1].value.f : args[1].value.i;
        float t = args[2].type == TYPE_FLOAT ? args[2].value.f : args[2].value.i;
        return (Value){TYPE_FLOAT, .value.f = a + (b - a) * t};
    }
    if (strcmp(name, "step") == 0 && arg_count >= 2) {
        float edge = args[0].type == TYPE_FLOAT ? args[0].value.f : args[0].value.i;
        float x = args[1].type == TYPE_FLOAT ? args[1].value.f : args[1].value.i;
        return (Value){TYPE_FLOAT, .value.f = x < edge ? 0.0f : 1.0f};
    }
    if (strcmp(name, "smoothstep") == 0 && arg_count >= 3) {
        float e0 = args[0].type == TYPE_FLOAT ? args[0].value.f : args[0].value.i;
        float e1 = args[1].type == TYPE_FLOAT ? args[1].value.f : args[1].value.i;
        float x = args[2].type == TYPE_FLOAT ? args[2].value.f : args[2].value.i;
        float t = clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
        return (Value){TYPE_FLOAT, .value.f = t * t * (3.0f - 2.0f * t)};
    }
    Function *func = find_func(interp, name);
    if (!func) {
        // Function not found, return zero
        return (Value){TYPE_INT, .value.i = 0};
    }
    
    // Parse parameters
    char param_names[10][32];
    int param_count = 0;
    char *params_copy = strdup(func->params);
    char *param = strtok(params_copy, ",");
    while (param && param_count < arg_count) {
        while (*param && isspace(*param)) param++;
        char *end = param;
        while (*end && !isspace(*end) && *end != ',') end++;
        *end = '\0';
        strcpy(param_names[param_count], param);
        param_count++;
        param = strtok(NULL, ",");
    }
    free(params_copy);
    
    // Set parameter variables
    for (int i = 0; i < param_count && i < arg_count; i++) {
        set_var(interp, param_names[i], args[i]);
    }
    
    // Execute function body (simplified - handle basic statements)
    Value result = {TYPE_INT, .value.i = 0};
    char *body_copy = strdup(func->body);
    char *line = strtok(body_copy, "\n");
    while (line) {
        char *trimmed = line;
        while (*trimmed && isspace(*trimmed)) trimmed++;
        
        // Handle return statement
        if (strncmp(trimmed, "return ", 7) == 0) {
            result = evaluate_expression(interp, trimmed + 7);
            break;
        }
        // Handle variable declarations and assignments
        else if (strstr(trimmed, " = ")) {
            char *eq_pos = strstr(trimmed, " = ");
            *eq_pos = '\0';
            char *var_name = trimmed;
            while (*var_name && isspace(*var_name)) var_name++;
            char *var_end = var_name;
            while (*var_end && (isalnum(*var_end) || *var_end == '_')) var_end++;
            *var_end = '\0';
            
            Value val = evaluate_expression(interp, eq_pos + 3);
            set_var(interp, var_name, val);
        }
        // Handle for loops (very basic)
        else if (strncmp(trimmed, "for ", 4) == 0) {
            // This is a simplified for loop parser
            // Format: for (int var = start; var <= end; var++)
            char *paren_start = strchr(trimmed, '(');
            if (paren_start) {
                char *int_pos = strstr(paren_start, "int ");
                if (int_pos) {
                    char *var_name = int_pos + 4;
                    while (*var_name && isspace(*var_name)) var_name++;
                    char *var_end = var_name;
                    while (*var_end && (isalnum(*var_end) || *var_end == '_')) var_end++;
                    *var_end = '\0';
                    
                    char *eq_pos = strchr(var_end + 1, '=');
                    if (eq_pos) {
                        Value start_val = evaluate_expression(interp, eq_pos + 1);
                        char *semi1 = strchr(eq_pos, ';');
                        if (semi1) {
                            char *le_pos = strstr(semi1, "<=");
                            if (le_pos) {
                                Value end_val = evaluate_expression(interp, le_pos + 2);
                                char *semi2 = strchr(le_pos, ';');
                                if (semi2) {
                                    // Execute loop body (simplified - assume single line for now)
                                    char *body_start = semi2 + 1;
                                    while (*body_start && *body_start != '{') body_start++;
                                    if (*body_start == '{') body_start++;
                                    
                                    int start_i = start_val.type == TYPE_FLOAT ? (int)floorf(start_val.value.f) : start_val.value.i;
                                    int end_i = end_val.type == TYPE_FLOAT ? (int)floorf(end_val.value.f) : end_val.value.i;
                                    for (int i = start_i; i <= end_i; i++) {
                                        set_var(interp, var_name, (Value){TYPE_INT, .value.i = i});
                                        // Execute loop body line by line
                                        char *loop_body = strdup(body_start);
                                        char *loop_line = strtok(loop_body, "\n");
                                        while (loop_line) {
                                            char *loop_trimmed = loop_line;
                                            while (*loop_trimmed && isspace(*loop_trimmed)) loop_trimmed++;
                                            if (*loop_trimmed && strncmp(loop_trimmed, "}", 1) != 0) {
                                                // Handle assignments in loop
                                                if (strstr(loop_trimmed, " += ")) {
                                                    char *plus_eq = strstr(loop_trimmed, " += ");
                                                    *plus_eq = '\0';
                                                    char *target = loop_trimmed;
                                                    while (*target && isspace(*target)) target++;
                                                    Value current = get_var(interp, target);
                                                    Value add_val = evaluate_expression(interp, plus_eq + 4);
                                                    if (current.type == TYPE_FLOAT && add_val.type == TYPE_FLOAT) {
                                                        current.value.f += add_val.value.f;
                                                    } else if (current.type == TYPE_INT && add_val.type == TYPE_INT) {
                                                        current.value.i += add_val.value.i;
                                                    }
                                                    set_var(interp, target, current);
                                                } else if (strstr(loop_trimmed, "++")) {
                                                    char *target = loop_trimmed;
                                                    while (*target && isspace(*target)) target++;
                                                    char *end = target;
                                                    while (*end && (isalnum(*end) || *end == '_')) end++;
                                                    *end = '\0';
                                                    Value current = get_var(interp, target);
                                                    if (current.type == TYPE_INT) {
                                                        current.value.i++;
                                                        set_var(interp, target, current);
                                                    }
                                                }
                                            }
                                            loop_line = strtok(NULL, "\n");
                                        }
                                        free(loop_body);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        
        line = strtok(NULL, "\n");
    }
    free(body_copy);
    
    return result;
}

static void set_var(Interpreter *interp, const char *name, Value value) {
    Variable *var = find_var(interp, name);
    if (var) {
        var->value = value;
    } else if (interp->var_count < MAX_VARS) {
        strcpy(interp->vars[interp->var_count].name, name);
        interp->vars[interp->var_count].value = value;
        interp->var_count++;
    }
}

static Value get_var(Interpreter *interp, const char *name) {
    Variable *var = find_var(interp, name);
    return var ? var->value : (Value){TYPE_INT, .value.i = 0};
}

static void parse_definitions(Interpreter *interp, const char *equation) {
    char *eq_copy = strdup(equation);
    char *line = strtok(eq_copy, "\n");
    
    while (line) {
        char *trimmed = line;
        while (*trimmed && isspace(*trimmed)) trimmed++;
        
        if (strncmp(trimmed, "deff ", 5) == 0) {
            char name[32];
            float value;
            if (sscanf(trimmed + 5, "%31s %f", name, &value) == 2) {
                set_var(interp, name, (Value){TYPE_FLOAT, .value.f = value});
            }
        } else if (strncmp(trimmed, "defi ", 5) == 0) {
            char name[32];
            int value;
            if (sscanf(trimmed + 5, "%31s %d", name, &value) == 2) {
                set_var(interp, name, (Value){TYPE_INT, .value.i = value});
            }
        } else if (strncmp(trimmed, "defn ", 5) == 0) {
            char name[32];
            char params[128];
            char *body_start = strchr(trimmed + 5, '{');
            if (body_start && interp->func_count < MAX_FUNCTIONS) {
                *body_start = '\0';
                if (sscanf(trimmed + 5, "%31s (%127[^)]", name, params) == 2) {
                    char *body_end = strrchr(body_start + 1, '}');
                    if (body_end) {
                        *body_end = '\0';
                        interp->functions[interp->func_count].body = strdup(body_start + 1);
                        strcpy(interp->functions[interp->func_count].name, name);
                        strcpy(interp->functions[interp->func_count].params, params);
                        interp->func_count++;
                    }
                }
            }
        }
        
        line = strtok(NULL, "\n");
    }
    
    free(eq_copy);
}

static void skip_ws(const char **expr) {
    while (**expr && isspace(**expr)) (*expr)++;
}

static float value_to_float(Value val) {
    return val.type == TYPE_FLOAT ? val.value.f : (float)val.value.i;
}

static Value parse_expression(Interpreter *interp, const char **expr);

static Value parse_primary(Interpreter *interp, const char **expr) {
    skip_ws(expr);

    if (**expr == '(') {
        (*expr)++;
        Value val = parse_expression(interp, expr);
        skip_ws(expr);
        if (**expr == ')') (*expr)++;
        return val;
    }

    if (**expr == '[') {
        (*expr)++;
        Value elements[16];
        int elem_count = 0;
        while (**expr && **expr != ']') {
            skip_ws(expr);
            elements[elem_count++] = parse_expression(interp, expr);
            skip_ws(expr);
            if (**expr == ',') (*expr)++;
        }
        if (**expr == ']') (*expr)++;

        Value result = {TYPE_ARRAY, .array_size = elem_count};
        result.value.arr = malloc(sizeof(float) * elem_count);
        for (int i = 0; i < elem_count; i++) {
            result.value.arr[i] = value_to_float(elements[i]);
        }
        return result;
    }

    if (isdigit(**expr) || **expr == '.') {
        const char *start = *expr;
        char *end = NULL;
        float val = strtof(*expr, &end);
        bool is_float = false;
        for (const char *p = start; p < end; p++) {
            if (*p == '.' || *p == 'e' || *p == 'E') {
                is_float = true;
                break;
            }
        }
        *expr = end;
        if (is_float) {
            return (Value){TYPE_FLOAT, .value.f = val};
        }
        return (Value){TYPE_INT, .value.i = (int)val};
    }

    if (isalpha(**expr) || **expr == '_') {
        char name[32];
        int i = 0;
        while ((isalnum(**expr) || **expr == '_') && i < 31) {
            name[i++] = **expr;
            (*expr)++;
        }
        name[i] = '\0';
        skip_ws(expr);

        if (**expr == '(') {
            (*expr)++;
            Value args[10];
            int arg_count = 0;
            while (**expr && **expr != ')') {
                skip_ws(expr);
                args[arg_count++] = parse_expression(interp, expr);
                skip_ws(expr);
                if (**expr == ',') (*expr)++;
            }
            if (**expr == ')') (*expr)++;
            return call_function(interp, name, args, arg_count);
        }

        if (**expr == '[') {
            (*expr)++;
            Value index = parse_expression(interp, expr);
            skip_ws(expr);
            if (**expr == ']') (*expr)++;

            int idx = (int)floorf(value_to_float(index));
            if (strcmp(name, "pixels") == 0) {
                if (idx >= 0 && idx < (int)(interp->width * interp->height * 4)) {
                    return (Value){TYPE_FLOAT, .value.f = interp->pixels[idx]};
                }
            }

            Value arr = get_var(interp, name);
            if (arr.type == TYPE_ARRAY) {
                if (idx >= 0 && idx < arr.array_size) {
                    return (Value){TYPE_FLOAT, .value.f = arr.value.arr[idx]};
                }
            }

            return (Value){TYPE_INT, .value.i = 0};
        }

        return get_var(interp, name);
    }

    return (Value){TYPE_INT, .value.i = 0};
}

static Value parse_unary(Interpreter *interp, const char **expr) {
    skip_ws(expr);
    if (**expr == '-') {
        (*expr)++;
        Value val = parse_unary(interp, expr);
        if (val.type == TYPE_FLOAT) {
            val.value.f = -val.value.f;
        } else {
            val.value.i = -val.value.i;
        }
        return val;
    }
    return parse_primary(interp, expr);
}

static Value parse_term(Interpreter *interp, const char **expr) {
    Value left = parse_unary(interp, expr);
    while (true) {
        skip_ws(expr);
        if (**expr == '*' || **expr == '/') {
            char op = **expr;
            (*expr)++;
            Value right = parse_unary(interp, expr);
            float a = value_to_float(left);
            float b = value_to_float(right);
            float result = op == '*' ? (a * b) : (b != 0.0f ? a / b : 0.0f);
            left = (Value){TYPE_FLOAT, .value.f = result};
        } else {
            break;
        }
    }
    return left;
}

static Value parse_expression(Interpreter *interp, const char **expr) {
    Value left = parse_term(interp, expr);
    while (true) {
        skip_ws(expr);
        if (**expr == '+' || **expr == '-') {
            char op = **expr;
            (*expr)++;
            Value right = parse_term(interp, expr);
            float a = value_to_float(left);
            float b = value_to_float(right);
            float result = op == '+' ? (a + b) : (a - b);
            left = (Value){TYPE_FLOAT, .value.f = result};
        } else {
            break;
        }
    }
    return left;
}

static Value evaluate_expression(Interpreter *interp, const char *expr) {
    return parse_expression(interp, &expr);
}

void apply_pixel_effect(uint8_t *pixels, size_t width, size_t height,
		const char *equation, double time_seconds) {
        Interpreter *interp = interpreter_create(pixels, width, height, time_seconds);
    
    // Parse definitions first
    parse_definitions(interp, equation);
    
    // For each pixel, execute the final assignments
    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width; x++) {
            interp->current_x = x;
            interp->current_y = y;
            
            size_t idx = (y * width + x) * 4;
            PixelVars vars = {
                .r = pixels[idx],
                .g = pixels[idx + 1],
                .b = pixels[idx + 2],
                .a = pixels[idx + 3]
            };
            
            // Set built-in variables
            set_var(interp, "x", (Value){TYPE_INT, .value.i = (int)x});
            set_var(interp, "y", (Value){TYPE_INT, .value.i = (int)y});
            set_var(interp, "width", (Value){TYPE_INT, .value.i = (int)width});
            set_var(interp, "height", (Value){TYPE_INT, .value.i = (int)height});
            set_var(interp, "time", (Value){TYPE_FLOAT, .value.f = (float)interp->time_seconds});
            set_var(interp, "pi", (Value){TYPE_FLOAT, .value.f = (float)M_PI});
            set_var(interp, "r", (Value){TYPE_FLOAT, .value.f = vars.r});
            set_var(interp, "g", (Value){TYPE_FLOAT, .value.f = vars.g});
            set_var(interp, "b", (Value){TYPE_FLOAT, .value.f = vars.b});
            set_var(interp, "a", (Value){TYPE_FLOAT, .value.f = vars.a});
            
            // Execute final equations (look for assignments and function calls)
            char *eq_copy = strdup(equation);
            char *line = strtok(eq_copy, "\n");
            while (line) {
                char *trimmed = line;
                while (*trimmed && isspace(*trimmed)) trimmed++;
                
                // Handle simple assignments
                if (strncmp(trimmed, "r = ", 4) == 0) {
                    Value val = evaluate_expression(interp, trimmed + 4);
                    pixels[idx] = (uint8_t)clamp(val.type == TYPE_FLOAT ? val.value.f : val.value.i, 0, 255);
                } else if (strncmp(trimmed, "g = ", 4) == 0) {
                    Value val = evaluate_expression(interp, trimmed + 4);
                    pixels[idx + 1] = (uint8_t)clamp(val.type == TYPE_FLOAT ? val.value.f : val.value.i, 0, 255);
                } else if (strncmp(trimmed, "b = ", 4) == 0) {
                    Value val = evaluate_expression(interp, trimmed + 4);
                    pixels[idx + 2] = (uint8_t)clamp(val.type == TYPE_FLOAT ? val.value.f : val.value.i, 0, 255);
                } else if (strncmp(trimmed, "a = ", 4) == 0) {
                    Value val = evaluate_expression(interp, trimmed + 4);
                    pixels[idx + 3] = (uint8_t)clamp(val.type == TYPE_FLOAT ? val.value.f : val.value.i, 0, 255);
                }

                // Handle chunk4 assignments (like from blur effect)
                else if (strstr(trimmed, "chunk4*:[r, g, b, a] = ") == trimmed) {
                    // Extract the function call part
                    char *call_start = strstr(trimmed, "= ") + 2;
                    Value result = evaluate_expression(interp, call_start);
                    
                    // Result should be an array of 4 floats
                    if (result.type == TYPE_ARRAY && result.array_size >= 4) {
                        pixels[idx] = (uint8_t)clamp(result.value.arr[0], 0, 255);
                        pixels[idx + 1] = (uint8_t)clamp(result.value.arr[1], 0, 255);
                        pixels[idx + 2] = (uint8_t)clamp(result.value.arr[2], 0, 255);
                        pixels[idx + 3] = (uint8_t)clamp(result.value.arr[3], 0, 255);
                    }
                }
                
                line = strtok(NULL, "\n");
            }
            free(eq_copy);
        }
    }
    
    interpreter_destroy(interp);
}

int send_event_to_client(struct IPCClient *client, uint16_t type, const void *payload, size_t payload_size) {
    uint32_t msg_length = sizeof(struct icm_ipc_header) + payload_size;
    uint16_t msg_type = type;
    uint16_t msg_flags = 0;
    uint32_t msg_sequence = 0;
    int32_t msg_num_fds = 0;

    uint8_t buffer[sizeof(struct icm_ipc_header) + payload_size];
    
    // Write header in little-endian format
    buffer[0] = msg_length & 0xFF;
    buffer[1] = (msg_length >> 8) & 0xFF;
    buffer[2] = (msg_length >> 16) & 0xFF;
    buffer[3] = (msg_length >> 24) & 0xFF;
    
    buffer[4] = msg_type & 0xFF;
    buffer[5] = (msg_type >> 8) & 0xFF;
    
    buffer[6] = msg_flags & 0xFF;
    buffer[7] = (msg_flags >> 8) & 0xFF;
    
    buffer[8] = msg_sequence & 0xFF;
    buffer[9] = (msg_sequence >> 8) & 0xFF;
    buffer[10] = (msg_sequence >> 16) & 0xFF;
    buffer[11] = (msg_sequence >> 24) & 0xFF;
    
    buffer[12] = msg_num_fds & 0xFF;
    buffer[13] = (msg_num_fds >> 8) & 0xFF;
    buffer[14] = (msg_num_fds >> 16) & 0xFF;
    buffer[15] = (msg_num_fds >> 24) & 0xFF;

    if (payload_size > 0) {
        memcpy(buffer + sizeof(struct icm_ipc_header), payload, payload_size);
    }

    size_t total_size = sizeof(struct icm_ipc_header) + payload_size;
    size_t sent_total = 0;
    
    /* Handle partial sends on non-blocking socket */
    while (sent_total < total_size) {
        ssize_t sent = send(client->socket_fd, buffer + sent_total, total_size - sent_total, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                /* Socket buffer is full, wait and retry */
                usleep(100);
                continue;
            }
            wlr_log(WLR_ERROR, "Failed to send event to client: %s", strerror(errno));
            return -1;
        }
        sent_total += sent;
    }
    
    return 0;
}

void ipc_server_broadcast_shutdown(struct IPCServer *ipc_server) {
    struct IPCClient *client, *tmp;
    wl_list_for_each_safe(client, tmp, &ipc_server->clients, link) {
        send_event_to_client(client, ICM_MSG_COMPOSITOR_SHUTDOWN, NULL, 0);
    }
}

void ipc_check_keybind(struct IPCServer *ipc_server, uint32_t modifiers, uint32_t keycode) {
    struct KeybindEntry *entry, *tmp;
    wl_list_for_each_safe(entry, tmp, &ipc_server->keybinds, link) {
        if (entry->modifiers == modifiers && entry->keycode == keycode) {
            struct icm_msg_keybind_event event = {
                .keybind_id = entry->keybind_id
            };
            if (send_event_to_client(entry->client, ICM_MSG_KEYBIND_EVENT, &event, sizeof(event)) < 0) {
                /* Client disconnected, it will be cleaned up elsewhere */
            }
        }
    }
}

void ipc_check_click_region(struct IPCServer *ipc_server, uint32_t window_id, int32_t x, int32_t y, uint32_t button, uint32_t state) {
    struct ClickRegion *region, *tmp;
    wl_list_for_each_safe(region, tmp, &ipc_server->click_regions, link) {
        if (region->window_id == window_id &&
            x >= region->x && x < region->x + (int32_t)region->width &&
            y >= region->y && y < region->y + (int32_t)region->height) {
            struct icm_msg_click_region_event event = {
                .region_id = region->region_id,
                .button = button,
                .state = state
            };
            if (send_event_to_client(region->client, ICM_MSG_CLICK_REGION_EVENT, &event, sizeof(event)) < 0) {
                /* Client disconnected, it will be cleaned up elsewhere */
            }
        }
    }
}

/* Unregister a window from all IPC clients that were listening for events on it */
void ipc_window_unmap(struct IPCServer *ipc_server, uint32_t window_id) {
    struct IPCClient *client, *tmp;
    wl_list_for_each_safe(client, tmp, &ipc_server->clients, link) {
        /* If this client was listening for events on this window, unregister it */
        if (client->event_window_id == window_id) {
            client->registered_pointer = 0;
            client->registered_keyboard = 0;
            client->event_window_id = 0;
            wlr_log(WLR_DEBUG, "Unregistered window %u from IPC client", window_id);
        }
    }
    
    /* Clean up click regions for this window */
    struct ClickRegion *region, *region_tmp;
    wl_list_for_each_safe(region, region_tmp, &ipc_server->click_regions, link) {
        if (region->window_id == window_id) {
            wl_list_remove(&region->link);
            free(region);
        }
    }
}

static ssize_t recv_with_fds(int socket_fd, void *data, size_t size,
                             int *fds, int *num_fds, int max_fds) {
    struct cmsghdr *cmsg;
    struct msghdr msg = {0};
    struct iovec iov = {
        .iov_base = data,
        .iov_len = size,
    };

    char cmsgbuf[CMSG_SPACE(max_fds * sizeof(int))];
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = cmsgbuf;
    msg.msg_controllen = sizeof(cmsgbuf);

    ssize_t ret = recvmsg(socket_fd, &msg, 0);
    if (ret < 0) {
        return ret;
    }

    *num_fds = 0;
    for (cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
            int n = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
            if (n > max_fds) n = max_fds;
            memcpy(fds, CMSG_DATA(cmsg), n * sizeof(int));
            *num_fds = n;
            break;
        }
    }

    return ret;
}

/* Buffer management */
struct BufferEntry *ipc_buffer_create(struct IPCServer *ipc_server, uint32_t buffer_id,
                                       int32_t width, int32_t height, uint32_t format) {
    struct BufferEntry *entry = calloc(1, sizeof(*entry));
    if (!entry) return NULL;

    entry->buffer_id = buffer_id;
    entry->x = 0;
    entry->y = 0;
    entry->width = width;
    entry->height = height;
    entry->format = format;
    entry->dmabuf_fd = -1;
    entry->visible = 1;
    entry->dirty = 0;
    entry->opacity = 1.0f;
    entry->blur_radius = 0.0f;
    entry->blur_enabled = 0;
    entry->effect_enabled = 0;
    entry->effect_dirty = 0;
    entry->use_effect_buffer = 0;
    entry->effect_equation[0] = '\0';
    entry->effect_data = NULL;
    entry->effect_data_size = 0;
    entry->has_transform_matrix = 0;
    entry->scale_x = 1.0f;
    entry->scale_y = 1.0f;
    entry->rotation = 0.0f;
    entry->num_planes = 0;

    /* Allocate CPU-accessible buffer */
    uint32_t stride = width * 4;  /* Assume RGBA */
    entry->size = stride * height;
    entry->data = malloc(entry->size);
    if (!entry->data) {
        free(entry);
        return NULL;
    }

    wl_list_insert(&ipc_server->buffers, &entry->link);
    return entry;
}

void ipc_buffer_destroy(struct IPCServer *ipc_server, uint32_t buffer_id) {
    struct BufferEntry *entry, *tmp;
    wl_list_for_each_safe(entry, tmp, &ipc_server->buffers, link) {
        if (entry->buffer_id == buffer_id) {
            wl_list_remove(&entry->link);
            if (entry->data) free(entry->data);
            if (entry->effect_data) free(entry->effect_data);
            if (entry->dmabuf_fd >= 0) close(entry->dmabuf_fd);
            if (entry->wlr_buffer) {
                wlr_buffer_drop(entry->wlr_buffer);
                entry->wlr_buffer = NULL;
            }
            free(entry);
            return;
        }
    }
}

struct BufferEntry *ipc_buffer_get(struct IPCServer *ipc_server, uint32_t buffer_id) {
    struct BufferEntry *entry;
    wl_list_for_each(entry, &ipc_server->buffers, link) {
        if (entry->buffer_id == buffer_id) {
            return entry;
        }
    }
    return NULL;
}

/* Helper: schedule frame redraw on all outputs */
static void schedule_frame_update(struct IPCServer *ipc_server) {
    struct wlr_scene_output *scene_output;
    wl_list_for_each(scene_output, &ipc_server->server->scene->outputs, link) {
        wlr_output_schedule_frame(scene_output->output);
    }
}

struct SceneOpacityData {
    float opacity;
    float blur_radius;
    uint8_t blur_enabled;
};

static void apply_scene_opacity_iter(struct wlr_scene_buffer *scene_buffer,
        int sx, int sy, void *data) {
    (void)sx;
    (void)sy;
    struct SceneOpacityData *state = data;
    float opacity = state->opacity;
    if (state->blur_enabled) {
        float blur_opacity = 1.0f - (state->blur_radius * 0.05f);
        if (blur_opacity < 0.5f) blur_opacity = 0.5f;
        opacity *= blur_opacity;
    }
    wlr_scene_buffer_set_opacity(scene_buffer, opacity);
}

struct SceneTransformData {
    float scale_x;
    float scale_y;
    enum wl_output_transform transform;
};

static void apply_scene_transform_iter(struct wlr_scene_buffer *scene_buffer,
        int sx, int sy, void *data) {
    (void)sx;
    (void)sy;
    struct SceneTransformData *state = data;
    int width = scene_buffer->buffer ? scene_buffer->buffer->width : scene_buffer->dst_width;
    int height = scene_buffer->buffer ? scene_buffer->buffer->height : scene_buffer->dst_height;
    if (width > 0 && height > 0) {
        wlr_scene_buffer_set_dest_size(scene_buffer,
            (int)(width * state->scale_x), (int)(height * state->scale_y));
    }
    wlr_scene_buffer_set_transform(scene_buffer, state->transform);
}

struct SceneMatrixData {
    float matrix[16];
    uint8_t has_matrix;
};

static void apply_scene_matrix_iter(struct wlr_scene_buffer *scene_buffer,
        int sx, int sy, void *data) {
    (void)sx;
    (void)sy;
    struct SceneMatrixData *state = data;
    if (state->has_matrix) {
        wlr_scene_buffer_set_transform_matrix(scene_buffer, state->matrix);
    } else {
        wlr_scene_buffer_clear_transform_matrix(scene_buffer);
    }
}

/* Helper: Draw a filled rectangle in RGBA buffer */
static void draw_rect_in_buffer(uint8_t *pixels, uint32_t width, uint32_t height,
                                int32_t x, int32_t y, uint32_t rect_width, uint32_t rect_height,
                                uint32_t color_rgba) {
    // Clamp rectangle to buffer bounds
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x + rect_width > width) rect_width = width - x;
    if (y + rect_height > height) rect_height = height - y;
    
    uint8_t r = (color_rgba >> 24) & 0xFF;
    uint8_t g = (color_rgba >> 16) & 0xFF;
    uint8_t b = (color_rgba >> 8) & 0xFF;
    uint8_t a = color_rgba & 0xFF;
    
    for (uint32_t row = y; row < y + rect_height; row++) {
        for (uint32_t col = x; col < x + rect_width; col++) {
            uint32_t idx = (row * width + col) * 4;
            
            // Blend with alpha
            if (a == 255) {
                pixels[idx] = r;
                pixels[idx + 1] = g;
                pixels[idx + 2] = b;
                pixels[idx + 3] = a;
            } else if (a > 0) {
                float alpha = a / 255.0f;
                pixels[idx] = (uint8_t)(pixels[idx] * (1 - alpha) + r * alpha);
                pixels[idx + 1] = (uint8_t)(pixels[idx + 1] * (1 - alpha) + g * alpha);
                pixels[idx + 2] = (uint8_t)(pixels[idx + 2] * (1 - alpha) + b * alpha);
                pixels[idx + 3] = (a > pixels[idx + 3]) ? a : pixels[idx + 3];
            }
        }
    }
}

/* Helper: Render decorations on a window buffer */
static void render_window_decorations(struct BufferEntry *buffer, struct IPCServer *ipc_server) {
    if (!buffer || !buffer->decorated || !buffer->data) {
        return;
    }
    
    uint32_t border_width = ipc_server->decoration_border_width;
    uint32_t title_height = ipc_server->decoration_title_height;
    uint32_t color = buffer->focused ? ipc_server->decoration_color_focus : ipc_server->decoration_color_unfocus;
    
    if (border_width == 0 && title_height == 0) {
        return;  // No decoration configured
    }
    
    // Draw title bar
    if (title_height > 0) {
        draw_rect_in_buffer(buffer->data, buffer->width, buffer->height,
                          0, 0, buffer->width, title_height, color);
    }
    
    // Draw borders
    if (border_width > 0) {
        // Top border (already drawn if title_height > 0)
        if (title_height == 0) {
            draw_rect_in_buffer(buffer->data, buffer->width, buffer->height,
                              0, 0, buffer->width, border_width, color);
        }
        
        // Bottom border
        draw_rect_in_buffer(buffer->data, buffer->width, buffer->height,
                          0, buffer->height - border_width, buffer->width, border_width, color);
        
        // Left border
        draw_rect_in_buffer(buffer->data, buffer->width, buffer->height,
                          0, 0, border_width, buffer->height, color);
        
        // Right border
        draw_rect_in_buffer(buffer->data, buffer->width, buffer->height,
                          buffer->width - border_width, 0, border_width, buffer->height, color);
    }
}

struct ImageEntry *ipc_image_create(struct IPCServer *ipc_server, uint32_t image_id,
                                    uint32_t width, uint32_t height, uint32_t format,
                                    const uint8_t *data, size_t data_size) {
    struct ImageEntry *entry = calloc(1, sizeof(*entry));
    if (!entry) return NULL;

    entry->image_id = image_id;
    entry->width = width;
    entry->height = height;
    entry->format = format;
    entry->data_size = data_size;
    entry->data = malloc(data_size);
    if (!entry->data) {
        free(entry);
        return NULL;
    }
    memcpy(entry->data, data, data_size);

    wl_list_insert(&ipc_server->images, &entry->link);
    return entry;
}

void ipc_image_destroy(struct IPCServer *ipc_server, uint32_t image_id) {
    struct ImageEntry *entry, *tmp;
    wl_list_for_each_safe(entry, tmp, &ipc_server->images, link) {
        if (entry->image_id == image_id) {
            wl_list_remove(&entry->link);
            if (entry->data) free(entry->data);
            free(entry);
            return;
        }
    }
}

struct ImageEntry *ipc_image_get(struct IPCServer *ipc_server, uint32_t image_id) {
    struct ImageEntry *entry;
    wl_list_for_each(entry, &ipc_server->images, link) {
        if (entry->image_id == image_id) {
            return entry;
        }
    }
    return NULL;
}

void ipc_client_disconnect(struct IPCClient *client) {
    if (!client) return;

    wl_list_remove(&client->link);

    /* Clean up keybinds */
    struct KeybindEntry *kb, *kb_tmp;
    wl_list_for_each_safe(kb, kb_tmp, &client->server->ipc_server.keybinds, link) {
        if (kb->client == client) {
            wl_list_remove(&kb->link);
            free(kb);
        }
    }

    /* Clean up click regions */
    struct ClickRegion *cr, *cr_tmp;
    wl_list_for_each_safe(cr, cr_tmp, &client->server->ipc_server.click_regions, link) {
        if (cr->client == client) {
            wl_list_remove(&cr->link);
            free(cr);
        }
    }

    /* Clean up screen copy requests */
    struct ScreenCopyRequest *scr, *scr_tmp;
    wl_list_for_each_safe(scr, scr_tmp, &client->server->ipc_server.screen_copy_requests, link) {
        if (scr->client == client) {
            wl_list_remove(&scr->link);
            free(scr);
        }
    }

    if (client->event_source) {
        wl_event_source_remove(client->event_source);
    }
    close(client->socket_fd);
    free(client);
}

/* Message handlers */
static int handle_import_dmabuf(struct IPCServer *ipc_server, struct IPCClient *client,
                                 const struct icm_msg_import_dmabuf *msg,
                                 const int *fds, int num_fds) {
    if (num_fds < msg->num_planes) {
        fprintf(stderr, "Not enough FDs for DMABUF planes\n");
        return -1;
    }

    struct BufferEntry *entry = ipc_buffer_create(ipc_server, msg->buffer_id,
                                                   msg->width, msg->height, msg->format);
    if (!entry) {
        return -1;
    }

    for (uint32_t i = 0; i < msg->num_planes && i < 4; i++) {
        entry->planes[i].fd = fds[i];
        entry->planes[i].offset = msg->planes[i].offset;
        entry->planes[i].stride = msg->planes[i].stride;
        entry->planes[i].modifier = msg->planes[i].modifier;
    }
    entry->num_planes = msg->num_planes;

    fprintf(stderr, "Imported DMABUF buffer %u (%dx%d format=%u)\n",
            msg->buffer_id, msg->width, msg->height, msg->format);
    return 0;
}

static int handle_draw_rect(struct IPCServer *ipc_server, struct IPCClient *client,
                             const struct icm_msg_draw_rect *msg) {
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (!buffer) {
        fprintf(stderr, "Buffer not found for window %u\n", msg->window_id);
        return -1;
    }

    /* Optimized RGBA rect fill using memset */
    uint32_t color = msg->color_rgba;
    uint8_t *ptr = (uint8_t *)buffer->data;
    uint32_t stride = buffer->width * 4;

    int32_t x1 = msg->x;
    int32_t y1 = msg->y;
    int32_t x2 = msg->x + msg->width;
    int32_t y2 = msg->y + msg->height;

    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > (int32_t)buffer->width) x2 = buffer->width;
    if (y2 > (int32_t)buffer->height) y2 = buffer->height;

    int32_t width = x2 - x1;
    if (width <= 0) return 0;

    for (int32_t y = y1; y < y2; y++) {
        uint8_t *row_start = ptr + y * stride + x1 * 4;
        uint32_t *pixel_ptr = (uint32_t *)row_start;
        for (int32_t x = 0; x < width; x++) {
            pixel_ptr[x] = color;
        }
    }

    buffer->dirty = 1;
    schedule_frame_update(ipc_server);
    return 0;
}

static int handle_draw_line(struct IPCServer *ipc_server, struct IPCClient *client,
                             const struct icm_msg_draw_line *msg) {
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (!buffer) {
        return -1;
    }

    /* Bresenham line drawing */
    uint32_t color = msg->color_rgba;
    uint8_t *ptr = (uint8_t *)buffer->data;
    uint32_t stride = buffer->width * 4;

    int dx = abs(msg->x1 - msg->x0);
    int dy = abs(msg->y1 - msg->y0);
    int sx = msg->x0 < msg->x1 ? 1 : -1;
    int sy = msg->y0 < msg->y1 ? 1 : -1;
    int err = dx - dy;

    int x = msg->x0, y = msg->y0;
    while (1) {
        if (x >= 0 && x < (int)buffer->width && y >= 0 && y < (int)buffer->height) {
            uint32_t *pixel = (uint32_t *)(ptr + y * stride + x * 4);
            *pixel = color;
        }

        if (x == msg->x1 && y == msg->y1) break;
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }

    buffer->dirty = 1;
    schedule_frame_update(ipc_server);
    return 0;
}

static int handle_draw_circle(struct IPCServer *ipc_server, struct IPCClient *client,
                               const struct icm_msg_draw_circle *msg) {
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (!buffer) {
        return -1;
    }

    uint32_t color = msg->color_rgba;
    uint8_t *ptr = (uint8_t *)buffer->data;
    uint32_t stride = buffer->width * 4;

    /* Midpoint circle algorithm */
    int x = 0;
    int y = msg->radius;
    int d = 3 - 2 * msg->radius;

    while (x <= y) {
        int points[8][2] = {
            {msg->cx + x, msg->cy + y},
            {msg->cx - x, msg->cy + y},
            {msg->cx + x, msg->cy - y},
            {msg->cx - x, msg->cy - y},
            {msg->cx + y, msg->cy + x},
            {msg->cx - y, msg->cy + x},
            {msg->cx + y, msg->cy - x},
            {msg->cx - y, msg->cy - x},
        };

        for (int i = 0; i < 8; i++) {
            int px = points[i][0];
            int py = points[i][1];
            if (px >= 0 && px < (int)buffer->width && py >= 0 && py < (int)buffer->height) {
                uint32_t *pixel = (uint32_t *)(ptr + py * stride + px * 4);
                *pixel = color;
            }
        }

        if (d < 0) {
            d = d + 4 * x + 6;
        } else {
            d = d + 4 * (x - y) + 10;
            y--;
        }
        x++;
    }

    buffer->dirty = 1;
    schedule_frame_update(ipc_server);
    return 0;
}

static int handle_draw_polygon(struct IPCServer *ipc_server, struct IPCClient *client,
                                uint8_t *payload, uint32_t payload_size) {
    if (payload_size < sizeof(struct icm_msg_draw_polygon)) {
        return -1;
    }

    struct icm_msg_draw_polygon *msg = (struct icm_msg_draw_polygon *)payload;
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (!buffer) {
        return -1;
    }

    uint32_t num_points = msg->num_points;
    if (num_points < 2) {
        return -1;
    }

    /* Points are stored right after the message structure */
    int32_t *points = (int32_t *)(payload + sizeof(struct icm_msg_draw_polygon));

    uint32_t color = msg->color_rgba;
    uint8_t *ptr = (uint8_t *)buffer->data;
    uint32_t stride = buffer->width * 4;

    /* Draw lines between consecutive points */
    for (uint32_t i = 0; i < num_points; i++) {
        int32_t x0 = points[i * 2];
        int32_t y0 = points[i * 2 + 1];
        int32_t x1 = points[((i + 1) % num_points) * 2];
        int32_t y1 = points[((i + 1) % num_points) * 2 + 1];

        /* Bresenham line */
        int dx = abs(x1 - x0);
        int dy = abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1;
        int sy = y0 < y1 ? 1 : -1;
        int err = dx - dy;

        int x = x0, y = y0;
        while (1) {
            if (x >= 0 && x < (int)buffer->width && y >= 0 && y < (int)buffer->height) {
                uint32_t *pixel = (uint32_t *)(ptr + y * stride + x * 4);
                *pixel = color;
            }

            if (x == x1 && y == y1) break;
            int e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                x += sx;
            }
            if (e2 < dx) {
                err += dx;
                y += sy;
            }
        }
    }

    buffer->dirty = 1;
    schedule_frame_update(ipc_server);
    return 0;
}

static int handle_create_buffer(struct IPCServer *ipc_server, struct IPCClient *client,
                                 const struct icm_msg_create_buffer *msg) {
    struct BufferEntry *entry = ipc_buffer_create(ipc_server, msg->buffer_id,
                                                   msg->width, msg->height, msg->format);
    if (!entry) {
        wlr_log(WLR_ERROR, "Failed to create buffer %u", msg->buffer_id);
        return -1;
    }

    fprintf(stderr, "Created buffer %u (%dx%d)\n", msg->buffer_id, msg->width, msg->height);

    /* Draw initial decorations if needed */
    draw_window_decorations(entry);
    
    /* Start fade-in animation */
    if (entry->opacity < 1.0f) {
        start_buffer_animation(entry, 300); // 300ms fade-in
        entry->target_opacity = 1.0f;
    }

    /* Send window created event to all clients */
    struct icm_msg_window_created event = {
        .window_id = msg->buffer_id,
        .width = msg->width,
        .height = msg->height,
        .decorated = entry->decorated,
        .focused = entry->focused
    };
    
    struct IPCClient *c, *tmp;
    wl_list_for_each_safe(c, tmp, &ipc_server->clients, link) {
        fprintf(stderr, "Sending WINDOW_CREATED event to client\n");
        if (send_event_to_client(c, ICM_MSG_WINDOW_CREATED, &event, sizeof(event)) < 0) {
            /* Client disconnected, will be cleaned up elsewhere */
        }
    }

    schedule_frame_update(ipc_server);
    return 0;
}

/* Draw window decorations (title bar, borders) */
void draw_window_decorations(struct BufferEntry *buffer) {
    if (!buffer || !buffer->decorated || !buffer->data || buffer->format != 0 || buffer->size == 0) {
        return;
    }

    uint32_t *pixels = (uint32_t *)buffer->data;
    uint32_t border_color = buffer->focused ? 0xFF4285F4 : 0xFFCCCCCC; // Blue when focused, gray when not
    uint32_t titlebar_color = buffer->focused ? 0xFF5C6BC0 : 0xFFE0E0E0;
    int border_width = 2;
    int titlebar_height = 24;

    // Draw title bar
    for (int y = 0; y < titlebar_height && y < buffer->height; y++) {
        for (int x = 0; x < buffer->width; x++) {
            int idx = y * buffer->width + x;
            if (idx < (int)(buffer->size / 4)) {
                pixels[idx] = titlebar_color;
            }
        }
    }

    // Draw borders
    for (int y = 0; y < buffer->height; y++) {
        for (int x = 0; x < buffer->width; x++) {
            int idx = y * buffer->width + x;
            if (idx >= (int)(buffer->size / 4)) continue;

            // Left border
            if (x < border_width) {
                pixels[idx] = border_color;
            }
            // Right border
            else if (x >= buffer->width - border_width) {
                pixels[idx] = border_color;
            }
            // Bottom border (skip titlebar area)
            else if (y >= buffer->height - border_width && y >= titlebar_height) {
                pixels[idx] = border_color;
            }
        }
    }

    buffer->dirty = 1;
}

/* Animation system */
void start_buffer_animation(struct BufferEntry *buffer, uint32_t duration_ms) {
    buffer->animating = 1;
    buffer->animation_start_time = 0; // Will be set on first frame
    buffer->animation_duration = duration_ms;
    
    // Store current values as start values
    buffer->start_opacity = buffer->opacity;
    buffer->start_scale_x = buffer->scale_x;
    buffer->start_scale_y = buffer->scale_y;
    buffer->start_x = buffer->x;
    buffer->start_y = buffer->y;
    
    // Set target values (these should be set by the caller)
    buffer->target_opacity = buffer->opacity;
    buffer->target_scale_x = buffer->scale_x;
    buffer->target_scale_y = buffer->scale_y;
    buffer->target_x = buffer->x;
    buffer->target_y = buffer->y;
}

void update_buffer_animation(struct BufferEntry *buffer, uint32_t current_time) {
    if (!buffer->animating) return;
    
    if (buffer->animation_start_time == 0) {
        buffer->animation_start_time = current_time;
        return;
    }
    
    uint32_t elapsed = current_time - buffer->animation_start_time;
    float progress = (float)elapsed / buffer->animation_duration;
    
    if (progress >= 1.0f) {
        // Animation complete
        buffer->opacity = buffer->target_opacity;
        buffer->scale_x = buffer->target_scale_x;
        buffer->scale_y = buffer->target_scale_y;
        buffer->x = buffer->target_x;
        buffer->y = buffer->target_y;
        // 3D transforms
        buffer->start_translate_x = buffer->target_translate_x;
        buffer->start_translate_y = buffer->target_translate_y;
        buffer->start_translate_z = buffer->target_translate_z;
        buffer->start_rotate_x = buffer->target_rotate_x;
        buffer->start_rotate_y = buffer->target_rotate_y;
        buffer->start_rotate_z = buffer->target_rotate_z;
        buffer->start_scale_z = buffer->target_scale_z;
        buffer->current_translate_x = buffer->target_translate_x;
        buffer->current_translate_y = buffer->target_translate_y;
        buffer->current_translate_z = buffer->target_translate_z;
        buffer->current_rotate_x = buffer->target_rotate_x;
        buffer->current_rotate_y = buffer->target_rotate_y;
        buffer->current_rotate_z = buffer->target_rotate_z;
        buffer->current_scale_z = buffer->target_scale_z;
        
        // Apply final 3D transform
        build_transform_matrix(buffer->transform_matrix, buffer->target_translate_x, buffer->target_translate_y, buffer->target_translate_z,
                              buffer->target_rotate_x, buffer->target_rotate_y, buffer->target_rotate_z,
                              buffer->target_scale_x, buffer->target_scale_y, buffer->target_scale_z);
        buffer->has_transform_matrix = 1;
        if (buffer->scene_buffer) {
            wlr_scene_buffer_set_transform_matrix(buffer->scene_buffer, buffer->transform_matrix);
        }
        
        buffer->animating = 0;
        buffer->dirty = 1;
        return;
    }
    
    // Ease-in-out interpolation
    float t = progress < 0.5f ? 2 * progress * progress : 1 - pow(-2 * progress + 2, 2) / 2;
    
    buffer->opacity = buffer->start_opacity + t * (buffer->target_opacity - buffer->start_opacity);
    buffer->scale_x = buffer->start_scale_x + t * (buffer->target_scale_x - buffer->start_scale_x);
    buffer->scale_y = buffer->start_scale_y + t * (buffer->target_scale_y - buffer->start_scale_y);
    buffer->x = buffer->start_x + t * (buffer->target_x - buffer->start_x);
    buffer->y = buffer->start_y + t * (buffer->target_y - buffer->start_y);
    
    // 3D interpolation
    buffer->current_translate_x = buffer->start_translate_x + t * (buffer->target_translate_x - buffer->start_translate_x);
    buffer->current_translate_y = buffer->start_translate_y + t * (buffer->target_translate_y - buffer->start_translate_y);
    buffer->current_translate_z = buffer->start_translate_z + t * (buffer->target_translate_z - buffer->start_translate_z);
    buffer->current_rotate_x = buffer->start_rotate_x + t * (buffer->target_rotate_x - buffer->start_rotate_x);
    buffer->current_rotate_y = buffer->start_rotate_y + t * (buffer->target_rotate_y - buffer->start_rotate_y);
    buffer->current_rotate_z = buffer->start_rotate_z + t * (buffer->target_rotate_z - buffer->start_rotate_z);
    buffer->current_scale_z = buffer->start_scale_z + t * (buffer->target_scale_z - buffer->start_scale_z);
    
    // Build and apply 3D transform matrix
    build_transform_matrix(buffer->transform_matrix, buffer->current_translate_x, buffer->current_translate_y, buffer->current_translate_z,
                          buffer->current_rotate_x, buffer->current_rotate_y, buffer->current_rotate_z,
                          buffer->scale_x, buffer->scale_y, buffer->current_scale_z);
    buffer->has_transform_matrix = 1;
    if (buffer->scene_buffer) {
        wlr_scene_buffer_set_transform_matrix(buffer->scene_buffer, buffer->transform_matrix);
    }
    
    buffer->dirty = 1;
}

/* Update all active animations */
void update_animations(struct IPCServer *ipc_server) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    uint32_t current_time = now.tv_sec * 1000 + now.tv_nsec / 1000000;
    
    struct BufferEntry *buffer, *tmp;
    wl_list_for_each_safe(buffer, tmp, &ipc_server->buffers, link) {
        if (buffer->animating) {
            update_buffer_animation(buffer, current_time);
        }
    }
}

static int handle_destroy_buffer(struct IPCServer *ipc_server, struct IPCClient *client,
                                  const struct icm_msg_destroy_buffer *msg) {
    ipc_buffer_destroy(ipc_server, msg->buffer_id);
    fprintf(stderr, "Destroyed buffer %u\n", msg->buffer_id);

    /* Send window destroyed event to all clients */
    struct icm_msg_window_destroyed event = {
        .window_id = msg->buffer_id
    };
    struct IPCClient *c, *tmp;
    wl_list_for_each_safe(c, tmp, &ipc_server->clients, link) {
        if (send_event_to_client(c, ICM_MSG_WINDOW_DESTROYED, &event, sizeof(event)) < 0) {
            /* Client disconnected, will be cleaned up elsewhere */
        }
    }

    schedule_frame_update(ipc_server);
    return 0;
}

struct ExportedSurface {
    struct wl_list link;
    uint32_t surface_id;             /* Unique surface identifier */
    uint32_t window_id;             /* Source window in this compositor */
    struct BufferEntry *buffer;     /* Rendering target buffer */
    struct View *view;              /* Associated view (optional) */
    uint8_t active;
};

static int handle_export_surface(struct IPCServer *ipc_server, struct IPCClient *client,
                                  const struct icm_msg_export_surface *msg) {
    /* Create a new exported surface entry */
    struct ExportedSurface *exported = calloc(1, sizeof(*exported));
    if (!exported) {
        return -1;
    }

    exported->surface_id = msg->surface_id;
    exported->window_id = msg->window_id;
    exported->active = 1;

    /* Create a buffer to render into */
    exported->buffer = ipc_buffer_create(ipc_server, msg->surface_id, 1280, 720, 0x34325241); /* ARGB */
    if (!exported->buffer) {
        free(exported);
        return -1;
    }

    wl_list_insert(&ipc_server->surfaces, &exported->link);
    fprintf(stderr, "Exported surface %u from window %u\n", msg->surface_id, msg->window_id);
    return 0;
}

static int handle_import_surface(struct IPCServer *ipc_server, struct IPCClient *client,
                                  const struct icm_msg_import_surface *msg) {
    /* In nested compositing, this would import a surface from another compositor
       For now, just track the import request */
    fprintf(stderr, "Imported surface %u to window %u\n", msg->surface_id, msg->window_id);
    return 0;
}

static int handle_register_pointer_event(struct IPCServer *ipc_server, struct IPCClient *client,
                                         const struct icm_msg_register_pointer_event *msg) {
    client->registered_pointer = 1;
    client->event_window_id = msg->window_id;
    fprintf(stderr, "Client registered for pointer events on window %u\n", msg->window_id);
    return 0;
}

static int handle_register_keyboard_event(struct IPCServer *ipc_server, struct IPCClient *client,
                                          const struct icm_msg_register_keyboard_event *msg) {
    client->registered_keyboard = 1;
    client->event_window_id = msg->window_id;
    fprintf(stderr, "Client registered for keyboard events on window %u\n", msg->window_id);
    return 0;
}

static int handle_query_capture_mouse(struct IPCServer *ipc_server, struct IPCClient *client,
                                      const struct icm_msg_query_capture_mouse *msg) {
    /* For now, assume capture is granted */
    fprintf(stderr, "Client queried capture mouse on window %u\n", msg->window_id);
    return 0;
}

static int handle_query_capture_keyboard(struct IPCServer *ipc_server, struct IPCClient *client,
                                         const struct icm_msg_query_capture_keyboard *msg) {
    /* For now, assume capture is granted */
    fprintf(stderr, "Client queried capture keyboard on window %u\n", msg->window_id);
    return 0;
}

static int handle_upload_image(struct IPCServer *ipc_server, struct IPCClient *client,
                               const struct icm_msg_upload_image *msg, size_t payload_size) {
    size_t expected_size = sizeof(*msg) + msg->data_size;
    if (payload_size < expected_size) {
        fprintf(stderr, "Incomplete upload_image message\n");
        return -1;
    }

    uint32_t image_id = ipc_server->next_image_id++;
    struct ImageEntry *entry = ipc_image_create(ipc_server, image_id,
                                                msg->width, msg->height, msg->format,
                                                msg->data, msg->data_size);
    if (!entry) {
        return -1;
    }

    fprintf(stderr, "Uploaded image %u (%dx%d format=%u size=%zu)\n",
            image_id, msg->width, msg->height, msg->format, msg->data_size);
    return 0;
}

static int handle_destroy_image(struct IPCServer *ipc_server, struct IPCClient *client,
                                const struct icm_msg_destroy_image *msg) {
    ipc_image_destroy(ipc_server, msg->image_id);
    fprintf(stderr, "Destroyed image %u\n", msg->image_id);
    return 0;
}

static int handle_draw_uploaded_image(struct IPCServer *ipc_server, struct IPCClient *client,
                                      const struct icm_msg_draw_uploaded_image *msg) {
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (!buffer) {
        fprintf(stderr, "Buffer not found for window %u\n", msg->window_id);
        return -1;
    }

    struct ImageEntry *image = ipc_image_get(ipc_server, msg->image_id);
    if (!image) {
        fprintf(stderr, "Image not found %u\n", msg->image_id);
        return -1;
    }

    /* Simple blit, assuming RGBA */
    uint8_t *dst_ptr = (uint8_t *)buffer->data;
    uint32_t dst_stride = buffer->width * 4;
    uint8_t *src_ptr = image->data;
    uint32_t src_stride = image->width * 4;

    int32_t dst_x = msg->x;
    int32_t dst_y = msg->y;
    uint32_t width = msg->width;
    uint32_t height = msg->height;
    uint32_t src_x = msg->src_x;
    uint32_t src_y = msg->src_y;
    uint32_t src_width = msg->src_width;
    uint32_t src_height = msg->src_height;
    uint8_t alpha = msg->alpha;

    if (dst_x < 0) { src_x -= dst_x; width += dst_x; dst_x = 0; }
    if (dst_y < 0) { src_y -= dst_y; height += dst_y; dst_y = 0; }
    if (dst_x + width > buffer->width) width = buffer->width - dst_x;
    if (dst_y + height > buffer->height) height = buffer->height - dst_y;
    if (src_x + width > image->width) width = image->width - src_x;
    if (src_y + height > image->height) height = image->height - src_y;

    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t *dst_pixel = (uint32_t *)(dst_ptr + (dst_y + y) * dst_stride + (dst_x + x) * 4);
            uint32_t *src_pixel = (uint32_t *)(src_ptr + (src_y + y) * src_stride + (src_x + x) * 4);
            uint32_t src_color = *src_pixel;
            uint32_t dst_color = *dst_pixel;

            // Simple alpha blend
            uint8_t sa = (src_color >> 24) & 0xFF;
            uint8_t sr = (src_color >> 16) & 0xFF;
            uint8_t sg = (src_color >> 8) & 0xFF;
            uint8_t sb = src_color & 0xFF;

            uint8_t da = (dst_color >> 24) & 0xFF;
            uint8_t dr = (dst_color >> 16) & 0xFF;
            uint8_t dg = (dst_color >> 8) & 0xFF;
            uint8_t db = dst_color & 0xFF;

            uint8_t a = (sa * alpha) / 255;
            uint8_t r = (sr * a + dr * (255 - a)) / 255;
            uint8_t g = (sg * a + dg * (255 - a)) / 255;
            uint8_t b = (sb * a + db * (255 - a)) / 255;

            *dst_pixel = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    buffer->dirty = 1;
    return 0;
}

static int handle_draw_text(struct IPCServer *ipc_server, struct IPCClient *client,
                            const struct icm_msg_draw_text *msg, size_t payload_size) {
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (!buffer) {
        fprintf(stderr, "Buffer not found for window %u\n", msg->window_id);
        return -1;
    }

    size_t text_len = payload_size - sizeof(*msg);
    if (text_len == 0) return 0;

    // Use Pango for proper text rendering if buffer supports it
    if (buffer->data && buffer->format == 0 && buffer->size >= (buffer->width * buffer->height * 4)) { // RGBA format
        // Create Cairo surface from buffer data
        cairo_surface_t *surface = cairo_image_surface_create_for_data(
            (unsigned char *)buffer->data,
            CAIRO_FORMAT_ARGB32,
            buffer->width,
            buffer->height,
            buffer->width * 4
        );

        if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
            fprintf(stderr, "Failed to create Cairo surface for text rendering\n");
            return -1;
        }

        cairo_t *cr = cairo_create(surface);
        if (cairo_status(cr) != CAIRO_STATUS_SUCCESS) {
            cairo_surface_destroy(surface);
            fprintf(stderr, "Failed to create Cairo context for text rendering\n");
            return -1;
        }

        // Set up Pango
        PangoLayout *layout = pango_cairo_create_layout(cr);
        if (!layout) {
            cairo_destroy(cr);
            cairo_surface_destroy(surface);
            fprintf(stderr, "Failed to create Pango layout for text rendering\n");
            return -1;
        }

        // Set font description
        char font_desc[256];
        snprintf(font_desc, sizeof(font_desc), "Sans %d", msg->font_size);
        PangoFontDescription *desc = pango_font_description_from_string(font_desc);
        pango_layout_set_font_description(layout, desc);
        pango_font_description_free(desc);

        // Set text
        pango_layout_set_text(layout, msg->text, -1);

        // Set text color
        double r = ((msg->color_rgba >> 16) & 0xFF) / 255.0;
        double g = ((msg->color_rgba >> 8) & 0xFF) / 255.0;
        double b = (msg->color_rgba & 0xFF) / 255.0;
        double a = ((msg->color_rgba >> 24) & 0xFF) / 255.0;
        cairo_set_source_rgba(cr, r, g, b, a);

        // Move to position and show text
        cairo_move_to(cr, msg->x, msg->y);
        pango_cairo_show_layout(cr, layout);

        // Clean up
        g_object_unref(layout);
        cairo_destroy(cr);
        cairo_surface_destroy(surface);

        fprintf(stderr, "Rendered text with Pango on window %u at (%d,%d): '%.100s'%s (color=0x%x, size=%d)\n",
                msg->window_id, msg->x, msg->y, msg->text,
                text_len > 100 ? "..." : "", msg->color_rgba, msg->font_size);
    } else {
        fprintf(stderr, "Cannot draw text on window %u: unsupported format or no buffer data\n", msg->window_id);
    }

    buffer->dirty = 1;
    schedule_frame_update(ipc_server);
    return 0;
}

static int handle_set_window_visible(struct IPCServer *ipc_server, struct IPCClient *client,
                                     const struct icm_msg_set_window_visible *msg) {
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (!buffer) {
        fprintf(stderr, "Buffer not found for window %u\n", msg->window_id);
        return -1;
    }

    buffer->visible = msg->visible;
    fprintf(stderr, "Set window %u visible: %d\n", msg->window_id, msg->visible);
    schedule_frame_update(ipc_server);

    return 0;
}

static int handle_register_keybind(struct IPCServer *ipc_server, struct IPCClient *client,
                                   const struct icm_msg_register_keybind *msg) {
    struct KeybindEntry *entry = calloc(1, sizeof(*entry));
    if (!entry) return -1;

    entry->keybind_id = msg->keybind_id;
    entry->modifiers = msg->modifiers;
    entry->keycode = msg->keycode;
    entry->client = client;

    wl_list_insert(&ipc_server->keybinds, &entry->link);
    fprintf(stderr, "Registered keybind %u (mod=%u key=%u)\n", msg->keybind_id, msg->modifiers, msg->keycode);
    return 0;
}

static int handle_unregister_keybind(struct IPCServer *ipc_server, struct IPCClient *client,
                                     const struct icm_msg_unregister_keybind *msg) {
    struct KeybindEntry *entry, *tmp;
    wl_list_for_each_safe(entry, tmp, &ipc_server->keybinds, link) {
        if (entry->keybind_id == msg->keybind_id && entry->client == client) {
            wl_list_remove(&entry->link);
            free(entry);
            fprintf(stderr, "Unregistered keybind %u\n", msg->keybind_id);
            return 0;
        }
    }
    return -1;
}

static int handle_register_click_region(struct IPCServer *ipc_server, struct IPCClient *client,
                                        const struct icm_msg_register_click_region *msg) {
    struct ClickRegion *region = calloc(1, sizeof(*region));
    if (!region) return -1;

    region->region_id = msg->region_id;
    region->window_id = msg->window_id;
    region->x = msg->x;
    region->y = msg->y;
    region->width = msg->width;
    region->height = msg->height;
    region->client = client;

    wl_list_insert(&ipc_server->click_regions, &region->link);
    fprintf(stderr, "Registered click region %u on window %u\n", msg->region_id, msg->window_id);
    return 0;
}

static int handle_unregister_click_region(struct IPCServer *ipc_server, struct IPCClient *client,
                                          const struct icm_msg_unregister_click_region *msg) {
    struct ClickRegion *region, *tmp;
    wl_list_for_each_safe(region, tmp, &ipc_server->click_regions, link) {
        if (region->region_id == msg->region_id && region->client == client) {
            wl_list_remove(&region->link);
            free(region);
            fprintf(stderr, "Unregistered click region %u\n", msg->region_id);
            return 0;
        }
    }
    return -1;
}

static int handle_request_screen_copy(struct IPCServer *ipc_server, struct IPCClient *client,
                                      const struct icm_msg_request_screen_copy *msg) {
    struct ScreenCopyRequest *req = calloc(1, sizeof(*req));
    if (!req) return -1;

    req->request_id = msg->request_id;
    req->x = msg->x;
    req->y = msg->y;
    req->width = msg->width;
    req->height = msg->height;
    req->client = client;

    wl_list_insert(&ipc_server->screen_copy_requests, &req->link);
    fprintf(stderr, "Queued screen copy request %u (%ux%u at %u,%u)\n", msg->request_id, msg->width, msg->height, msg->x, msg->y);
    return 0;
}

static int handle_register_global_pointer_event(struct IPCServer *ipc_server, struct IPCClient *client) {
    client->registered_global_pointer = 1;
    fprintf(stderr, "Client registered for global pointer events\n");
    return 0;
}

static int handle_register_global_keyboard_event(struct IPCServer *ipc_server, struct IPCClient *client) {
    client->registered_global_keyboard = 1;
    fprintf(stderr, "Client registered for global keyboard events\n");
    return 0;
}

static int handle_register_global_capture_mouse(struct IPCServer *ipc_server, struct IPCClient *client) {
    client->registered_global_capture_mouse = 1;
    fprintf(stderr, "Client registered for global mouse capture\n");
    return 0;
}

static int handle_register_global_capture_keyboard(struct IPCServer *ipc_server, struct IPCClient *client) {
    client->registered_global_capture_keyboard = 1;
    fprintf(stderr, "Client registered for global keyboard capture\n");
    return 0;
}

static int handle_unregister_global_capture_keyboard(struct IPCServer *ipc_server, struct IPCClient *client) {
    client->registered_global_capture_keyboard = 0;
    fprintf(stderr, "Client unregistered from global keyboard capture\n");
    return 0;
}

static int handle_unregister_global_capture_mouse(struct IPCServer *ipc_server, struct IPCClient *client) {
    client->registered_global_capture_mouse = 0;
    fprintf(stderr, "Client unregistered from global mouse capture\n");
    return 0;
}

static int handle_set_window_layer(struct IPCServer *ipc_server, struct IPCClient *client,
                                   const struct icm_msg_set_window_layer *msg) {
    /* Map IPC layer values to scene layer trees */
    int scene_layer;
    switch (msg->layer) {
    case 0: scene_layer = LyrBg; break;
    case 1: scene_layer = LyrBottom; break;
    case 2: scene_layer = LyrNormal; break;
    case 3: scene_layer = LyrTop; break;
    case 4: /* fall-through: overlay */
    default: scene_layer = LyrOverlay; break;
    }

    /* Try IPC buffer first */
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (buffer) {
        buffer->layer = msg->layer;
        if (buffer->scene_buffer) {
            wlr_scene_node_reparent(&buffer->scene_buffer->node, layers[scene_layer]);
        }
        schedule_frame_update(ipc_server);
        fprintf(stderr, "Set IPC buffer %u layer to %d (scene layer %d)\n",
                msg->window_id, msg->layer, scene_layer);
        return 0;
    }

    /* Try View (XDG toplevel) */
    struct Server *server = wl_container_of(ipc_server, server, ipc_server);
    struct View *view;
    wl_list_for_each(view, &server->views, link) {
        if (view->window_id == msg->window_id) {
            if (view->scene_tree) {
                wlr_scene_node_reparent(&view->scene_tree->node, layers[scene_layer]);
            }
            schedule_frame_update(ipc_server);
            fprintf(stderr, "Set View %u layer to %d (scene layer %d)\n",
                    msg->window_id, msg->layer, scene_layer);
            return 0;
        }
    }

    /* Try LayerSurface */
    struct LayerSurface *layer_surf;
    wl_list_for_each(layer_surf, &server->layer_surfaces, link) {
        if (layer_surf->window_id == msg->window_id) {
            if (layer_surf->scene_layer) {
                wlr_scene_node_reparent(&layer_surf->scene_layer->tree->node, layers[scene_layer]);
            }
            schedule_frame_update(ipc_server);
            fprintf(stderr, "Set LayerSurface %u layer to %d (scene layer %d)\n",
                    msg->window_id, msg->layer, scene_layer);
            return 0;
        }
    }

    fprintf(stderr, "Window %u not found for layer change\n", msg->window_id);
    return -1;
}

static int handle_raise_window(struct IPCServer *ipc_server, struct IPCClient *client,
                               const struct icm_msg_raise_window *msg) {
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (buffer && buffer->scene_buffer) {
        wlr_scene_node_raise_to_top(&buffer->scene_buffer->node);
        schedule_frame_update(ipc_server);
        fprintf(stderr, "Raised IPC buffer %u\n", msg->window_id);
        return 0;
    }

    struct Server *server = wl_container_of(ipc_server, server, ipc_server);
    struct View *view;
    wl_list_for_each(view, &server->views, link) {
        if (view->window_id == msg->window_id && view->scene_tree) {
            wlr_scene_node_raise_to_top(&view->scene_tree->node);
            schedule_frame_update(ipc_server);
            fprintf(stderr, "Raised View %u\n", msg->window_id);
            return 0;
        }
    }

    fprintf(stderr, "Window %u not found for raise\n", msg->window_id);
    return -1;
}

static int handle_lower_window(struct IPCServer *ipc_server, struct IPCClient *client,
                               const struct icm_msg_lower_window *msg) {
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (buffer && buffer->scene_buffer) {
        wlr_scene_node_lower_to_bottom(&buffer->scene_buffer->node);
        schedule_frame_update(ipc_server);
        fprintf(stderr, "Lowered IPC buffer %u\n", msg->window_id);
        return 0;
    }

    struct Server *server = wl_container_of(ipc_server, server, ipc_server);
    struct View *view;
    wl_list_for_each(view, &server->views, link) {
        if (view->window_id == msg->window_id && view->scene_tree) {
            wlr_scene_node_lower_to_bottom(&view->scene_tree->node);
            schedule_frame_update(ipc_server);
            fprintf(stderr, "Lowered View %u\n", msg->window_id);
            return 0;
        }
    }

    fprintf(stderr, "Window %u not found for lower\n", msg->window_id);
    return -1;
}

static int handle_set_window_parent(struct IPCServer *ipc_server, struct IPCClient *client,
                                    const struct icm_msg_set_window_parent *msg) {
    // Set window parent for hierarchical management
    fprintf(stderr, "Setting window %u parent to %u\n", msg->window_id, msg->parent_id);
    return 0;
}

/* Build a 4x4 transformation matrix from 3D parameters */
void build_transform_matrix(float *matrix, float tx, float ty, float tz,
                           float rx, float ry, float rz, float sx, float sy, float sz) {
    /* Start with identity matrix */
    for (int i = 0; i < 16; i++) matrix[i] = 0.0f;
    matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f;
    
    /* Apply scale */
    matrix[0] *= sx;
    matrix[5] *= sy;
    matrix[10] *= sz;
    
    /* Apply Z-axis rotation */
    float rad_z = rz * M_PI / 180.0f;
    float cos_z = cosf(rad_z);
    float sin_z = sinf(rad_z);
    float temp[16];
    memcpy(temp, matrix, sizeof(temp));
    matrix[0] = temp[0] * cos_z - temp[4] * sin_z;
    matrix[1] = temp[1] * cos_z - temp[5] * sin_z;
    matrix[4] = temp[0] * sin_z + temp[4] * cos_z;
    matrix[5] = temp[1] * sin_z + temp[5] * cos_z;
    
    /* Apply Y-axis rotation */
    float rad_y = ry * M_PI / 180.0f;
    float cos_y = cosf(rad_y);
    float sin_y = sinf(rad_y);
    memcpy(temp, matrix, sizeof(temp));
    matrix[0] = temp[0] * cos_y + temp[8] * sin_y;
    matrix[2] = -temp[0] * sin_y + temp[8] * cos_y;
    matrix[8] = temp[4] * cos_y + temp[8] * sin_y;
    matrix[10] = -temp[4] * sin_y + temp[10] * cos_y;
    
    /* Apply X-axis rotation */
    float rad_x = rx * M_PI / 180.0f;
    float cos_x = cosf(rad_x);
    float sin_x = sinf(rad_x);
    memcpy(temp, matrix, sizeof(temp));
    matrix[5] = temp[5] * cos_x - temp[9] * sin_x;
    matrix[6] = temp[6] * cos_x - temp[10] * sin_x;
    matrix[9] = temp[5] * sin_x + temp[9] * cos_x;
    matrix[10] = temp[6] * sin_x + temp[10] * cos_x;
    
    /* Apply translation */
    matrix[12] = tx;
    matrix[13] = ty;
    matrix[14] = tz;
}

static int handle_set_window_transform_3d(struct IPCServer *ipc_server, struct IPCClient *client,
                                          const struct icm_msg_set_window_transform_3d *msg) {
    float matrix[16];
    build_transform_matrix(matrix, msg->translate_x, msg->translate_y, msg->translate_z,
                          msg->rotate_x, msg->rotate_y, msg->rotate_z,
                          msg->scale_x, msg->scale_y, msg->scale_z);
    
    /* Now apply the matrix using the existing matrix system */
    struct icm_msg_set_window_matrix matrix_msg = {
        .window_id = msg->window_id
    };
    memcpy(matrix_msg.matrix, matrix, sizeof(matrix));
    
    fprintf(stderr, "Setting window %u 3D transform: translate(%.2f,%.2f,%.2f) rotate(%.2f,%.2f,%.2f) scale(%.2f,%.2f,%.2f)\n",
            msg->window_id, msg->translate_x, msg->translate_y, msg->translate_z,
            msg->rotate_x, msg->rotate_y, msg->rotate_z,
            msg->scale_x, msg->scale_y, msg->scale_z);
    
    return handle_set_window_matrix(ipc_server, client, &matrix_msg);
}

static int handle_set_window_matrix(struct IPCServer *ipc_server, struct IPCClient *client,
                                    const struct icm_msg_set_window_matrix *msg) {
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (buffer) {
        memcpy(buffer->transform_matrix, msg->matrix, sizeof(buffer->transform_matrix));
        buffer->has_transform_matrix = 1;
        if (buffer->scene_buffer) {
            wlr_scene_buffer_set_transform_matrix(buffer->scene_buffer, buffer->transform_matrix);
        }
        schedule_frame_update(ipc_server);
        fprintf(stderr, "Set IPC buffer %u transformation matrix\n", msg->window_id);
        return 0;
    }

    struct Server *server = wl_container_of(ipc_server, server, ipc_server);
    struct View *view;
    wl_list_for_each(view, &server->views, link) {
        if (view->window_id == msg->window_id && view->scene_tree) {
            memcpy(view->transform_matrix, msg->matrix, sizeof(view->transform_matrix));
            view->has_transform_matrix = 1;
            struct SceneMatrixData state = { .has_matrix = 1 };
            memcpy(state.matrix, msg->matrix, sizeof(state.matrix));
            wlr_scene_node_for_each_buffer(&view->scene_tree->node,
                apply_scene_matrix_iter, &state);
            schedule_frame_update(ipc_server);
            fprintf(stderr, "Set view %u transformation matrix\n", msg->window_id);
            return 0;
        }
    }

    fprintf(stderr, "Window %u not found for matrix transform\n", msg->window_id);
    return -1;
}

static int handle_set_window_mesh_transform(struct IPCServer *ipc_server, struct IPCClient *client,
                                            const struct icm_msg_set_window_mesh_transform *msg,
                                            const uint8_t *payload_data, size_t payload_size) {
    // Calculate expected payload size
    size_t header_size = sizeof(struct icm_msg_set_window_mesh_transform);
    size_t vertex_count = msg->mesh_width * msg->mesh_height;
    size_t expected_vertices_size = vertex_count * sizeof(struct icm_msg_mesh_vertex);
    
    if (payload_size < header_size + expected_vertices_size) {
        fprintf(stderr, "Mesh transform payload too small: got %zu, expected %zu\n",
                payload_size, header_size + expected_vertices_size);
        return -1;
    }
    
    // Get vertex data
    const struct icm_msg_mesh_vertex *vertices = 
        (const struct icm_msg_mesh_vertex *)(payload_data + header_size);
    
    // Find the view/buffer
    struct Server *server = wl_container_of(ipc_server, server, ipc_server);
    struct View *view;
    wl_list_for_each(view, &server->views, link) {
        if (view->window_id == msg->window_id) {
            // Free old mesh if exists
            if (view->mesh_transform.vertices) {
                free(view->mesh_transform.vertices);
            }
            
            // Allocate and copy new mesh
            view->mesh_transform.vertices = malloc(expected_vertices_size);
            if (!view->mesh_transform.vertices) {
                fprintf(stderr, "Failed to allocate mesh vertices\n");
                return -1;
            }
            
            memcpy(view->mesh_transform.vertices, vertices, expected_vertices_size);
            view->mesh_transform.mesh_width = msg->mesh_width;
            view->mesh_transform.mesh_height = msg->mesh_height;
            view->mesh_transform.enabled = 1;
            
            schedule_frame_update(ipc_server);
            fprintf(stderr, "Set mesh transform for window %u: %ux%u grid (%zu vertices)\n",
                    msg->window_id, msg->mesh_width, msg->mesh_height, vertex_count);
            return 0;
        }
    }
    
    fprintf(stderr, "Window %u not found for mesh transform\n", msg->window_id);
    return -1;
}

static int handle_clear_window_mesh_transform(struct IPCServer *ipc_server, struct IPCClient *client,
                                               const struct icm_msg_clear_window_mesh_transform *msg) {
    struct Server *server = wl_container_of(ipc_server, server, ipc_server);
    struct View *view;
    wl_list_for_each(view, &server->views, link) {
        if (view->window_id == msg->window_id) {
            if (view->mesh_transform.vertices) {
                free(view->mesh_transform.vertices);
                view->mesh_transform.vertices = NULL;
            }
            view->mesh_transform.mesh_width = 0;
            view->mesh_transform.mesh_height = 0;
            view->mesh_transform.enabled = 0;
            
            schedule_frame_update(ipc_server);
            fprintf(stderr, "Cleared mesh transform for window %u\n", msg->window_id);
            return 0;
        }
    }
    
    fprintf(stderr, "Window %u not found for clearing mesh transform\n", msg->window_id);
    return -1;
}

static int handle_update_window_mesh_vertices(struct IPCServer *ipc_server, struct IPCClient *client,
                                               const struct icm_msg_update_window_mesh_vertices *msg,
                                               const uint8_t *payload_data, size_t payload_size) {
    size_t header_size = sizeof(struct icm_msg_update_window_mesh_vertices);
    size_t expected_vertices_size = msg->num_vertices * sizeof(struct icm_msg_mesh_vertex);
    
    if (payload_size < header_size + expected_vertices_size) {
        fprintf(stderr, "Mesh update payload too small\n");
        return -1;
    }
    
    const struct icm_msg_mesh_vertex *new_vertices = 
        (const struct icm_msg_mesh_vertex *)(payload_data + header_size);
    
    struct Server *server = wl_container_of(ipc_server, server, ipc_server);
    struct View *view;
    wl_list_for_each(view, &server->views, link) {
        if (view->window_id == msg->window_id && view->mesh_transform.enabled) {
            size_t total_vertices = view->mesh_transform.mesh_width * view->mesh_transform.mesh_height;
            
            if (msg->start_index + msg->num_vertices > total_vertices) {
                fprintf(stderr, "Mesh update out of bounds\n");
                return -1;
            }
            
            // Update the specified vertices
            memcpy(&view->mesh_transform.vertices[msg->start_index], new_vertices, expected_vertices_size);
            
            schedule_frame_update(ipc_server);
            fprintf(stderr, "Updated %u mesh vertices for window %u starting at index %u\n",
                    msg->num_vertices, msg->window_id, msg->start_index);
            return 0;
        }
    }
    
    fprintf(stderr, "Window %u not found or mesh not enabled for vertex update\n", msg->window_id);
    return -1;
}

static int handle_set_window_state(struct IPCServer *ipc_server, struct IPCClient *client,
                                   const struct icm_msg_set_window_state *msg) {
    // State bitfield: 1=minimized, 2=maximized, 4=fullscreen, 8=decorated
    
    // First try to find as a BufferEntry (IPC-created window)
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (buffer) {
        buffer->minimized = (msg->state & 1) ? 1 : 0;
        buffer->maximized = (msg->state & 2) ? 1 : 0;
        buffer->fullscreen = (msg->state & 4) ? 1 : 0;
        buffer->decorated = (msg->state & 8) ? 1 : 0;
        
        // Hide/show based on minimized state
        if (buffer->scene_buffer) {
            if (buffer->minimized) {
                wlr_scene_node_set_enabled(&buffer->scene_buffer->node, false);
            } else {
                wlr_scene_node_set_enabled(&buffer->scene_buffer->node, true);
            }
        }
        
        schedule_frame_update(ipc_server);
        fprintf(stderr, "Set BufferEntry %u state: minimized=%d maximized=%d fullscreen=%d decorated=%d\n",
                msg->window_id, buffer->minimized, buffer->maximized, buffer->fullscreen, buffer->decorated);
        return 0;
    }
    
    // Also check for Views (xdg_toplevel windows like Qt)
    struct Server *server = wl_container_of(ipc_server, server, ipc_server);
    struct View *view;
    wl_list_for_each(view, &server->views, link) {
        if (view->window_id == msg->window_id) {
            // For xdg_toplevel windows, send appropriate state changes
            if (!view->is_xwayland && view->xdg_surface && view->xdg_surface->toplevel) {
                // Set window state using wlroots API
                if (msg->state & 2) {  // maximized
                    wlr_xdg_toplevel_set_maximized(view->xdg_surface->toplevel, true);
                } else {
                    wlr_xdg_toplevel_set_maximized(view->xdg_surface->toplevel, false);
                }
                
                if (msg->state & 4) {  // fullscreen
                    wlr_xdg_toplevel_set_fullscreen(view->xdg_surface->toplevel, true);
                } else {
                    wlr_xdg_toplevel_set_fullscreen(view->xdg_surface->toplevel, false);
                }
                
                fprintf(stderr, "Set View %u state: minimized=%d maximized=%d fullscreen=%d\n",
                        msg->window_id, (msg->state & 1) ? 1 : 0, (msg->state & 2) ? 1 : 0, (msg->state & 4) ? 1 : 0);
            }
            
            schedule_frame_update(ipc_server);
            return 0;
        }
    }
    
    // Check layer surfaces
    struct LayerSurface *layer_surf;
    wl_list_for_each(layer_surf, &server->layer_surfaces, link) {
        if (layer_surf->window_id == msg->window_id) {
            // Layer surfaces handle their own state; we can note decoration preference
            fprintf(stderr, "Set LayerSurface %u state: decorated=%d (layer surfaces manage own state)\n",
                    msg->window_id, (msg->state & 8) ? 1 : 0);
            return 0;
        }
    }
    
    fprintf(stderr, "Window %u not found for state change\n", msg->window_id);
    return -1;
}

static int handle_focus_window(struct IPCServer *ipc_server, struct IPCClient *client,
                               const struct icm_msg_focus_window *msg) {
    // Focus the specified window properly - raise it, activate it, and give it keyboard focus
    
    struct Server *server = wl_container_of(ipc_server, server, ipc_server);
    uint32_t old_focused_id = server->focused_window_id;
    server->focused_window_id = msg->window_id;
    
    // Find the view to focus
    struct View *view_to_focus = NULL;
    struct View *old_focused_view = NULL;
    struct wlr_surface *target_surface = NULL;
    
    // First, find the views
    struct View *v;
    wl_list_for_each(v, &server->views, link) {
        if (v->window_id == msg->window_id) {
            view_to_focus = v;
            target_surface = v->is_xwayland ? v->xwayland_surface->surface : v->xdg_surface->surface;
        }
        if (v->window_id == old_focused_id) {
            old_focused_view = v;
        }
    }
    
    // Check if the view is mapped before trying to focus
    if (view_to_focus && !view_to_focus->mapped) {
        fprintf(stderr, "Cannot focus window %u - not yet mapped\n", msg->window_id);
        return -1;
    }
    
    if (!view_to_focus) {
        // Not a regular view, might be a BufferEntry or LayerSurface
        struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
        if (buffer) {
            buffer->focused = 1;
            
            // Clear keyboard focus from any Wayland surface, allowing keyboard events via IPC
            struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
            if (keyboard && server->seat->keyboard_state.focused_surface) {
                wlr_seat_keyboard_clear_focus(server->seat);
            }
            
            fprintf(stderr, "Focused BufferEntry window %u (cleared Wayland surface focus, keyboard via IPC)\n", msg->window_id);
            schedule_frame_update(ipc_server);
            return 0;
        }
        
        struct LayerSurface *layer_surf;
        wl_list_for_each(layer_surf, &server->layer_surfaces, link) {
            if (layer_surf->window_id == msg->window_id) {
                // Set keyboard focus to the layer surface (fixes launcher input)
                struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
                if (keyboard && layer_surf->layer_surface && layer_surf->layer_surface->surface) {
                    wlr_seat_keyboard_notify_enter(server->seat, layer_surf->layer_surface->surface,
                        keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
                }
                fprintf(stderr, "Focused LayerSurface window %u (set keyboard focus)\n", msg->window_id);
                return 0;
            }
        }
        
        fprintf(stderr, "Window %u to focus not found\n", msg->window_id);
        return -1;
    }
    
    // We have a view to focus - do the full focus procedure
    
    // 1. Raise to top
    if (view_to_focus->scene_tree) {
        wlr_scene_node_raise_to_top(&view_to_focus->scene_tree->node);
    }
    
    // 2. Move to front of list
    wl_list_remove(&view_to_focus->link);
    wl_list_insert(&server->views, &view_to_focus->link);
    
    // 3. Deactivate old focused view
    if (old_focused_view) {
        if (old_focused_view->is_xwayland) {
            wlr_xwayland_surface_activate(old_focused_view->xwayland_surface, false);
        } else if (old_focused_view->xdg_surface && old_focused_view->xdg_surface->toplevel) {
            wlr_xdg_toplevel_set_activated(old_focused_view->xdg_surface->toplevel, false);
        }
        
        struct BufferEntry *old_buffer = ipc_buffer_get(ipc_server, old_focused_id);
        if (old_buffer) {
            old_buffer->focused = 0;
            draw_window_decorations(old_buffer);  // Redraw decorations for unfocused window
        }
    }
    
    // 4. Activate new view
    if (view_to_focus->is_xwayland) {
        wlr_xwayland_surface_activate(view_to_focus->xwayland_surface, true);
    } else if (view_to_focus->xdg_surface && view_to_focus->xdg_surface->toplevel) {
        wlr_xdg_toplevel_set_activated(view_to_focus->xdg_surface->toplevel, true);
    }
    
    // Mark as focused in buffer entries
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (buffer) {
        buffer->focused = 1;
        draw_window_decorations(buffer);  // Redraw decorations for focused window
    }
    
    // 5. Give keyboard focus
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
    if (keyboard && target_surface) {
        wlr_seat_keyboard_notify_enter(server->seat, target_surface,
            keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
    }
    
    fprintf(stderr, "Focused and raised window %u\n", msg->window_id);
    schedule_frame_update(ipc_server);
    return 0;
}

static int handle_blur_window(struct IPCServer *ipc_server, struct IPCClient *client,
                              const struct icm_msg_blur_window *msg) {
    // Blur (unfocus) the specified window
    
    struct Server *server = wl_container_of(ipc_server, server, ipc_server);
    
    // Find the view to blur
    struct View *view_to_blur = NULL;
    struct wlr_surface *old_surface = NULL;
    
    struct View *v;
    wl_list_for_each(v, &server->views, link) {
        if (v->window_id == msg->window_id) {
            view_to_blur = v;
            old_surface = v->is_xwayland ? v->xwayland_surface->surface : v->xdg_surface->surface;
            break;
        }
    }
    
    if (!view_to_blur) {
        // Not a regular view, might be a BufferEntry or LayerSurface
        struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
        if (buffer) {
            buffer->focused = 0;
            fprintf(stderr, "Blurred BufferEntry window %u\n", msg->window_id);
            schedule_frame_update(ipc_server);
            return 0;
        }
        
        struct LayerSurface *layer_surf;
        wl_list_for_each(layer_surf, &server->layer_surfaces, link) {
            if (layer_surf->window_id == msg->window_id) {
                // Clear keyboard focus from the layer surface
                struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
                if (keyboard && server->seat->keyboard_state.focused_surface == layer_surf->layer_surface->surface) {
                    wlr_seat_keyboard_clear_focus(server->seat);
                }
                fprintf(stderr, "Blurred LayerSurface window %u\n", msg->window_id);
                return 0;
            }
        }
        
        fprintf(stderr, "Window %u to blur not found\n", msg->window_id);
        return -1;
    }
    
    // We have a view to blur
    
    // Deactivate the view
    if (view_to_blur->is_xwayland) {
        wlr_xwayland_surface_activate(view_to_blur->xwayland_surface, false);
    } else if (view_to_blur->xdg_surface && view_to_blur->xdg_surface->toplevel) {
        wlr_xdg_toplevel_set_activated(view_to_blur->xdg_surface->toplevel, false);
    }
    
    // Mark as not focused in buffer entries
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (buffer) {
        buffer->focused = 0;
        draw_window_decorations(buffer);  // Redraw decorations for unfocused window
    }
    
    // Clear keyboard focus
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(server->seat);
    if (keyboard && server->seat->keyboard_state.focused_surface == old_surface) {
        wlr_seat_keyboard_clear_focus(server->seat);
    }
    
    fprintf(stderr, "Blurred window %u\n", msg->window_id);
    schedule_frame_update(ipc_server);
    return 0;
}

static int handle_animate_window(struct IPCServer *ipc_server, struct IPCClient *client,
                                 const struct icm_msg_animate_window *msg) {
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (!buffer) {
        fprintf(stderr, "Buffer not found for animation %u\n", msg->window_id);
        return -1;
    }
    
    /* Set up animation */
    buffer->animating = 1;
    buffer->animation_start_time = 0; // Will be set on first update
    buffer->animation_duration = msg->duration_ms;
    
    /* Store current values as start values */
    buffer->start_x = buffer->x;
    buffer->start_y = buffer->y;
    buffer->start_scale_x = buffer->scale_x;
    buffer->start_scale_y = buffer->scale_y;
    buffer->start_opacity = buffer->opacity;
    
    // 3D transforms - assume current transform is identity if not set
    buffer->start_translate_x = 0.0f;
    buffer->start_translate_y = 0.0f;
    buffer->start_translate_z = 0.0f;
    buffer->start_rotate_x = 0.0f;
    buffer->start_rotate_y = 0.0f;
    buffer->start_rotate_z = 0.0f;
    buffer->start_scale_z = 1.0f;
    
    // Initialize current values
    buffer->current_translate_x = buffer->start_translate_x;
    buffer->current_translate_y = buffer->start_translate_y;
    buffer->current_translate_z = buffer->start_translate_z;
    buffer->current_rotate_x = buffer->start_rotate_x;
    buffer->current_rotate_y = buffer->start_rotate_y;
    buffer->current_rotate_z = buffer->start_rotate_z;
    buffer->current_scale_z = buffer->start_scale_z;
    
    /* Set target values based on flags */
    if (msg->flags & 1) { // animate position
        buffer->target_x = msg->target_x;
        buffer->target_y = msg->target_y;
    } else {
        buffer->target_x = buffer->x;
        buffer->target_y = buffer->y;
    }
    
    if (msg->flags & 2) { // animate scale
        buffer->target_scale_x = msg->target_scale_x;
        buffer->target_scale_y = msg->target_scale_y;
    } else {
        buffer->target_scale_x = buffer->scale_x;
        buffer->target_scale_y = buffer->scale_y;
    }
    
    if (msg->flags & 4) { // animate opacity
        buffer->target_opacity = msg->target_opacity;
    } else {
        buffer->target_opacity = buffer->opacity;
    }
    
    if (msg->flags & 8) { // animate 3d translate
        buffer->target_translate_x = msg->target_translate_x;
        buffer->target_translate_y = msg->target_translate_y;
        buffer->target_translate_z = msg->target_translate_z;
    } else {
        buffer->target_translate_x = buffer->start_translate_x;
        buffer->target_translate_y = buffer->start_translate_y;
        buffer->target_translate_z = buffer->start_translate_z;
    }
    
    if (msg->flags & 16) { // animate 3d rotate
        buffer->target_rotate_x = msg->target_rotate_x;
        buffer->target_rotate_y = msg->target_rotate_y;
        buffer->target_rotate_z = msg->target_rotate_z;
    } else {
        buffer->target_rotate_x = buffer->start_rotate_x;
        buffer->target_rotate_y = buffer->start_rotate_y;
        buffer->target_rotate_z = buffer->start_rotate_z;
    }
    
    if (msg->flags & 32) { // animate 3d scale
        buffer->target_scale_z = msg->target_scale_z;
    } else {
        buffer->target_scale_z = buffer->start_scale_z;
    }
    
    fprintf(stderr, "Started animation for window %u: duration=%ums flags=%u\n",
            msg->window_id, msg->duration_ms, msg->flags);
    
    schedule_frame_update(ipc_server);
    return 0;
}

static int handle_stop_animation(struct IPCServer *ipc_server, struct IPCClient *client,
                                const struct icm_msg_stop_animation *msg) {
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (!buffer) {
        fprintf(stderr, "Buffer not found for stop animation %u\n", msg->window_id);
        return -1;
    }
    
    buffer->animating = 0;
    fprintf(stderr, "Stopped animation for window %u\n", msg->window_id);
    
    schedule_frame_update(ipc_server);
    return 0;
}

static int handle_set_window_position(struct IPCServer *ipc_server, struct IPCClient *client,
                                      const struct icm_msg_set_window_position *msg) {
    struct Server *server = wl_container_of(ipc_server, server, ipc_server);
    
    // First try to find as a BufferEntry (IPC-created window)
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (buffer) {
        buffer->x = msg->x;
        buffer->y = msg->y;
        
        // Update scene node immediately if it exists
        if (buffer->scene_buffer) {
            wlr_scene_node_set_position(&buffer->scene_buffer->node, buffer->x, buffer->y);
        }
        return 0;
    }
    
    // Try to find as a View (application window)
    struct View *view;
    wl_list_for_each(view, &server->views, link) {
        if (view->window_id == msg->window_id) {
            view->x = msg->x;
            view->y = msg->y;
            view->position_set_by_ipc = true;  /* Mark as IPC-positioned */
            if (view->scene_tree) {
                wlr_scene_node_set_position(&view->scene_tree->node, view->x, view->y);
            }
            fprintf(stderr, "Set view window %u position to (%d, %d) via IPC\n", msg->window_id, msg->x, msg->y);
            return 0;
        }
    }
        
    // Also check layer surfaces
    struct LayerSurface *layer_surf;
    wl_list_for_each(layer_surf, &server->layer_surfaces, link) {
        if (layer_surf->window_id == msg->window_id) {
            // Layer surfaces positioning is handled differently
            if (layer_surf->scene_layer) {
                wlr_scene_node_set_position(&layer_surf->scene_layer->tree->node, msg->x, msg->y);
            }
            schedule_frame_update(ipc_server);
            fprintf(stderr, "Set LayerSurface window %u position to (%d, %d) via IPC\n", msg->window_id, msg->x, msg->y);
            return 0;
        }
    }
    
    fprintf(stderr, "Window %u not found for positioning\n", msg->window_id);
    return -1;
}

static int handle_set_window_size(struct IPCServer *ipc_server, struct IPCClient *client,
                                  const struct icm_msg_set_window_size *msg) {
    // First try to find as a BufferEntry (IPC-created window)
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (buffer) {
        // Note: This doesn't resize the buffer data, just updates the logical size
        buffer->width = msg->width;
        buffer->height = msg->height;
        
        // Update scene buffer destination size immediately if it exists
        if (buffer->scene_buffer) {
            wlr_scene_buffer_set_dest_size(buffer->scene_buffer, 
                buffer->width * buffer->scale_x, buffer->height * buffer->scale_y);
        }
        
        fprintf(stderr, "Set IPC window %u size to %ux%u\n", msg->window_id, msg->width, msg->height);
        return 0;
    }
    
    // If not found as BufferEntry, try to find as a View (xdg_toplevel window like Qt)
    struct Server *server = wl_container_of(ipc_server, server, ipc_server);
    struct View *view;
    wl_list_for_each(view, &server->views, link) {
        if (view->window_id == msg->window_id) {
            // For xdg_toplevel windows, we need to send a configure event
            if (!view->is_xwayland && view->xdg_surface && view->xdg_surface->toplevel) {
                wlr_xdg_toplevel_set_size(view->xdg_surface->toplevel, msg->width, msg->height);
                fprintf(stderr, "Set View window %u size to %ux%u (xdg_toplevel)\n", msg->window_id, msg->width, msg->height);
            }
            return 0;
        }
    }
    
    // Layer surfaces don't support arbitrary resizing
    fprintf(stderr, "Window %u not found or cannot be resized\n", msg->window_id);
    return -1;
}

static int handle_set_window_opacity(struct IPCServer *ipc_server, struct IPCClient *client,
                                     const struct icm_msg_set_window_opacity *msg) {
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (!buffer) {
        struct Server *server = wl_container_of(ipc_server, server, ipc_server);
        struct View *view;
        wl_list_for_each(view, &server->views, link) {
            if (view->window_id == msg->window_id && view->scene_tree) {
                view->opacity = msg->opacity;
                struct SceneOpacityData state = {
                    .opacity = view->opacity,
                    .blur_radius = view->blur_radius,
                    .blur_enabled = view->blur_enabled,
                };
                wlr_scene_node_for_each_buffer(&view->scene_tree->node,
                    apply_scene_opacity_iter, &state);
                schedule_frame_update(ipc_server);
                fprintf(stderr, "Set view %u opacity to %f\n", msg->window_id, msg->opacity);
                return 0;
            }
        }

        struct LayerSurface *layer_surf;
        wl_list_for_each(layer_surf, &server->layer_surfaces, link) {
            if (layer_surf->window_id == msg->window_id && layer_surf->scene_layer) {
                struct SceneOpacityData state = {
                    .opacity = msg->opacity,
                    .blur_radius = 0.0f,
                    .blur_enabled = 0,
                };
                wlr_scene_node_for_each_buffer(&layer_surf->scene_layer->tree->node,
                    apply_scene_opacity_iter, &state);
                schedule_frame_update(ipc_server);
                fprintf(stderr, "Set layer surface %u opacity to %f\n", msg->window_id, msg->opacity);
                return 0;
            }
        }

        fprintf(stderr, "Window %u not found for opacity change\n", msg->window_id);
        return -1;
    }

    buffer->opacity = msg->opacity;
    
    // Update scene buffer opacity immediately if it exists
    if (buffer->scene_buffer) {
        wlr_scene_buffer_set_opacity(buffer->scene_buffer, buffer->opacity);
    }
    
    schedule_frame_update(ipc_server);
    fprintf(stderr, "Set window %u opacity to %f\n", msg->window_id, msg->opacity);
    return 0;
}

static int handle_set_window_blur(struct IPCServer *ipc_server, struct IPCClient *client,
                                   const struct icm_msg_set_window_blur *msg) {
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (!buffer) {
        struct Server *server = wl_container_of(ipc_server, server, ipc_server);
        struct View *view;
        wl_list_for_each(view, &server->views, link) {
            if (view->window_id == msg->window_id && view->scene_tree) {
                view->blur_radius = msg->blur_radius;
                view->blur_enabled = msg->enabled;
                struct SceneOpacityData state = {
                    .opacity = view->opacity,
                    .blur_radius = view->blur_radius,
                    .blur_enabled = view->blur_enabled,
                };
                wlr_scene_node_for_each_buffer(&view->scene_tree->node,
                    apply_scene_opacity_iter, &state);
                schedule_frame_update(ipc_server);
                fprintf(stderr, "Set view %u blur: radius=%f enabled=%d\n",
                        msg->window_id, msg->blur_radius, msg->enabled);
                return 0;
            }
        }

        fprintf(stderr, "Window %u not found for blur change\n", msg->window_id);
        return -1;
    }

    buffer->blur_radius = msg->blur_radius;
    buffer->blur_enabled = msg->enabled;
    
    // Note: wlroots doesn't have built-in blur support yet
    // This sets the values for future implementation or external compositor effects
    // For now, we can simulate blur effect by reducing opacity slightly
    if (buffer->blur_enabled && buffer->scene_buffer) {
        // Apply a subtle transparency effect as blur approximation
        float blur_opacity = 1.0f - (msg->blur_radius * 0.05f);
        if (blur_opacity < 0.5f) blur_opacity = 0.5f;
        wlr_scene_buffer_set_opacity(buffer->scene_buffer, buffer->opacity * blur_opacity);
    } else if (buffer->scene_buffer) {
        wlr_scene_buffer_set_opacity(buffer->scene_buffer, buffer->opacity);
    }
    
    schedule_frame_update(ipc_server);
    fprintf(stderr, "Set window %u blur: radius=%f enabled=%d\n", 
            msg->window_id, msg->blur_radius, msg->enabled);
    return 0;
}

static int handle_set_screen_effect(struct IPCServer *ipc_server, struct IPCClient *client,
                                    const struct icm_msg_set_screen_effect *msg) {
    strncpy(ipc_server->screen_effect_equation, msg->equation, sizeof(ipc_server->screen_effect_equation) - 1);
    ipc_server->screen_effect_equation[sizeof(ipc_server->screen_effect_equation) - 1] = '\0';
    ipc_server->screen_effect_enabled = msg->enabled;
    ipc_server->screen_effect_dirty = 1;
    
    schedule_frame_update(ipc_server);
    fprintf(stderr, "Set screen effect: equation='%s' enabled=%d\n", 
            msg->equation, msg->enabled);
    return 0;
}

static int handle_set_window_effect(struct IPCServer *ipc_server, struct IPCClient *client,
                                    const struct icm_msg_set_window_effect *msg) {
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (!buffer) {
        fprintf(stderr, "Buffer not found for window %u effect\n", msg->window_id);
        return -1;
    }

    strncpy(buffer->effect_equation, msg->equation, sizeof(buffer->effect_equation) - 1);
    buffer->effect_equation[sizeof(buffer->effect_equation) - 1] = '\0';
    buffer->effect_enabled = msg->enabled;
    buffer->effect_dirty = 1;

    schedule_frame_update(ipc_server);
    fprintf(stderr, "Set window %u effect: equation='%s' enabled=%d\n",
            msg->window_id, msg->equation, msg->enabled);
    return 0;
}

static int handle_set_window_transform(struct IPCServer *ipc_server, struct IPCClient *client,
                                       const struct icm_msg_set_window_transform *msg) {
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (!buffer) {
        struct Server *server = wl_container_of(ipc_server, server, ipc_server);
        struct View *view;
        wl_list_for_each(view, &server->views, link) {
            if (view->window_id == msg->window_id && view->scene_tree) {
                view->scale_x = msg->scale_x;
                view->scale_y = msg->scale_y;
                view->rotation = msg->rotation;

                enum wl_output_transform transform = WL_OUTPUT_TRANSFORM_NORMAL;
                if (fabsf(msg->rotation) >= 45.0f && fabsf(msg->rotation) < 135.0f) {
                    transform = WL_OUTPUT_TRANSFORM_90;
                } else if (fabsf(msg->rotation) >= 135.0f && fabsf(msg->rotation) < 225.0f) {
                    transform = WL_OUTPUT_TRANSFORM_180;
                } else if (fabsf(msg->rotation) >= 225.0f) {
                    transform = WL_OUTPUT_TRANSFORM_270;
                }

                struct SceneTransformData state = {
                    .scale_x = view->scale_x,
                    .scale_y = view->scale_y,
                    .transform = transform,
                };
                wlr_scene_node_for_each_buffer(&view->scene_tree->node,
                    apply_scene_transform_iter, &state);

                schedule_frame_update(ipc_server);
                fprintf(stderr, "Set view %u transform: scale %fx%f, rotation %f\n",
                        msg->window_id, msg->scale_x, msg->scale_y, msg->rotation);
                return 0;
            }
        }

        fprintf(stderr, "Window %u not found for transform\n", msg->window_id);
        return -1;
    }

    buffer->scale_x = msg->scale_x;
    buffer->scale_y = msg->scale_y;
    buffer->rotation = msg->rotation;
    
    schedule_frame_update(ipc_server);
    
    // Update scene buffer destination size immediately if it exists
    if (buffer->scene_buffer) {
        wlr_scene_buffer_set_dest_size(buffer->scene_buffer, 
            buffer->width * buffer->scale_x, buffer->height * buffer->scale_y);
    }
    
    fprintf(stderr, "Set window %u transform: scale %fx%f, rotation %f\n", msg->window_id, msg->scale_x, msg->scale_y, msg->rotation);
    return 0;
}

static int handle_query_window_position(struct IPCServer *ipc_server, struct IPCClient *client,
                                        const struct icm_msg_query_window_position *msg) {
    // Check buffers first (ICM client windows)
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (buffer) {
        struct icm_msg_window_position_data response = {
            .window_id = msg->window_id,
            .x = buffer->x,
            .y = buffer->y
        };
        send_event_to_client(client, ICM_MSG_WINDOW_POSITION_DATA, &response, sizeof(response));
        return 0;
    }

    // Note: Cannot iterate layer surfaces here - would need safe iteration
    // Returning error for now
    return -1;
}

static int handle_query_window_size(struct IPCServer *ipc_server, struct IPCClient *client,
                                    const struct icm_msg_query_window_size *msg) {
    // Check buffers first (ICM client windows)
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (buffer) {
        struct icm_msg_window_size_data response = {
            .window_id = msg->window_id,
            .width = buffer->width,
            .height = buffer->height
       };
        send_event_to_client(client, ICM_MSG_WINDOW_SIZE_DATA, &response, sizeof(response));
        return 0;
    }

    wlr_log(WLR_ERROR, "Query window size: buffer not found for window %u", msg->window_id);
    return -1;
}

static int handle_query_window_attributes(struct IPCServer *ipc_server, struct IPCClient *client,
                                          const struct icm_msg_query_window_attributes *msg) {
    // Check buffers (ICM client windows)
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (buffer) {
        struct icm_msg_window_attributes_data response = {
            .window_id = msg->window_id,
            .visible = buffer->visible,
            .opacity = buffer->opacity,
            .scale_x = buffer->scale_x,
            .scale_y = buffer->scale_y,
            .rotation = buffer->rotation
        };
        send_event_to_client(client, ICM_MSG_WINDOW_ATTRIBUTES_DATA, &response, sizeof(response));
        return 0;
    }

    // Return default attributes for XDG surfaces and layer surfaces
    struct icm_msg_window_attributes_data response = {
        .window_id = msg->window_id,
        .visible = 1,
        .opacity = 1.0f,
        .scale_x = 1.0f,
        .scale_y = 1.0f,
        .rotation = 0.0f
    };
    send_event_to_client(client, ICM_MSG_WINDOW_ATTRIBUTES_DATA, &response, sizeof(response));
    return 0;
}

static int handle_query_window_layer(struct IPCServer *ipc_server, struct IPCClient *client,
                                     const struct icm_msg_query_window_layer *msg) {
    // For now, return default layer information
    // In a full implementation, this would check actual layer assignments
    struct icm_msg_window_layer_data response = {
        .window_id = msg->window_id,
        .layer = 0, // Default layer
        .parent_id = 0 // Root parent
    };
    send_event_to_client(client, ICM_MSG_WINDOW_LAYER_DATA, &response, sizeof(response));
    return 0;
}

static int handle_query_window_state(struct IPCServer *ipc_server, struct IPCClient *client,
                                     const struct icm_msg_query_window_state *msg) {
    // Check buffers for ICM client windows
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (buffer) {
        uint32_t state = 0;
        if (buffer->minimized) state |= 1;
        if (buffer->maximized) state |= 2;
        if (buffer->fullscreen) state |= 4;
        if (buffer->decorated) state |= 8;

        struct icm_msg_window_state_data response = {
            .window_id = msg->window_id,
            .state = state,
            .focused = buffer->focused
        };
        send_event_to_client(client, ICM_MSG_WINDOW_STATE_DATA, &response, sizeof(response));
        return 0;
    }

    // For XDG and layer surfaces, return default state
    struct icm_msg_window_state_data response = {
        .window_id = msg->window_id,
        .state = 8, // Decorated by default
        .focused = 0
    };
    send_event_to_client(client, ICM_MSG_WINDOW_STATE_DATA, &response, sizeof(response));
    return 0;
}

static void get_output_layout_dimensions(struct IPCServer *ipc_server,
                                         int *width,
                                         int *height,
                                         float *scale) {
    struct wlr_box box;
    wlr_output_layout_get_box(ipc_server->server->output_layout, NULL, &box);

    int total_width = box.width;
    int total_height = box.height;
    float max_scale = 1.0f;

    int has_output = 0;
    int min_x = 0;
    int min_y = 0;
    int max_x = 0;
    int max_y = 0;

    struct wlr_output_layout_output *output;
    wl_list_for_each(output, &ipc_server->server->output_layout->outputs, link) {
        if (!output->output) {
            continue;
        }

        if (output->output->scale > max_scale) {
            max_scale = output->output->scale;
        }

        struct wlr_output_layout_output *lo = wlr_output_layout_get(ipc_server->server->output_layout,
                                                                    output->output);
        if (!lo) {
            continue;
        }

        int eff_width = 0;
        int eff_height = 0;
        wlr_output_effective_resolution(output->output, &eff_width, &eff_height);

        int left = lo->x;
        int top = lo->y;
        int right = lo->x + eff_width;
        int bottom = lo->y + eff_height;

        if (!has_output) {
            min_x = left;
            min_y = top;
            max_x = right;
            max_y = bottom;
            has_output = 1;
        } else {
            if (left < min_x) min_x = left;
            if (top < min_y) min_y = top;
            if (right > max_x) max_x = right;
            if (bottom > max_y) max_y = bottom;
        }
    }

    if (total_width <= 0 || total_height <= 0) {
        if (has_output) {
            total_width = max_x - min_x;
            total_height = max_y - min_y;
        }
    }

    if (total_width <= 0) total_width = 1920;
    if (total_height <= 0) total_height = 1080;

    *width = total_width;
    *height = total_height;
    *scale = max_scale;
}

static int handle_query_screen_dimensions(struct IPCServer *ipc_server, struct IPCClient *client,
                                         const struct icm_msg_query_screen_dimensions *msg) {
    int total_width = 0;
    int total_height = 0;
    float scale = 1.0f;

    get_output_layout_dimensions(ipc_server, &total_width, &total_height, &scale);
    
    struct icm_msg_screen_dimensions_data response = {
        .total_width = (uint32_t)total_width,
        .total_height = (uint32_t)total_height,
        .scale = scale
    };
    send_event_to_client(client, ICM_MSG_SCREEN_DIMENSIONS_DATA, &response, sizeof(response));
    return 0;
}

static int handle_query_monitors(struct IPCServer *ipc_server, struct IPCClient *client,
                                const struct icm_msg_query_monitors *msg) {
    struct wl_list *output_list = &ipc_server->server->output_layout->outputs;
    uint32_t num_monitors = 0;
    struct wlr_output_layout_output *output;

    wl_list_for_each(output, &ipc_server->server->output_layout->outputs, link) {
        num_monitors++;
    }
    
    // Allocate buffer for response
    size_t response_size = sizeof(struct icm_msg_monitors_data) + 
                          (num_monitors * sizeof(struct icm_msg_monitor_info));
    uint8_t *response_buf = malloc(response_size);
    
    struct icm_msg_monitors_data *response = (struct icm_msg_monitors_data *)response_buf;
    response->num_monitors = num_monitors;
    
    struct icm_msg_monitor_info *monitor_info = 
        (struct icm_msg_monitor_info *)(response_buf + sizeof(struct icm_msg_monitors_data));
    
    int idx = 0;
    int primary_set = 0;
    wl_list_for_each(output, output_list, link) {
        struct wlr_output_layout_output *lo = wlr_output_layout_get(ipc_server->server->output_layout, output->output);
        int eff_width = 0;
        int eff_height = 0;
        if (output->output) {
            wlr_output_effective_resolution(output->output, &eff_width, &eff_height);
        }
        
        monitor_info[idx].x = lo ? lo->x : 0;
        monitor_info[idx].y = lo ? lo->y : 0;
        monitor_info[idx].width = (uint32_t)eff_width;
        monitor_info[idx].height = (uint32_t)eff_height;
        monitor_info[idx].physical_width = output->output->phys_width;
        monitor_info[idx].physical_height = output->output->phys_height;
        monitor_info[idx].refresh_rate = output->output->refresh;
        monitor_info[idx].scale = output->output->scale;
        monitor_info[idx].enabled = output->output->enabled;
        if (!primary_set && output->output->enabled) {
            monitor_info[idx].primary = 1;
            primary_set = 1;
        } else {
            monitor_info[idx].primary = 0;
        }
        
        // Copy monitor name
        snprintf(monitor_info[idx].name, sizeof(monitor_info[idx].name), "%s", output->output->name);
        
        idx++;
    }
    
    send_event_to_client(client, ICM_MSG_MONITORS_DATA, response_buf, response_size);
    free(response_buf);
    return 0;
}

/*
struct icm_msg_window_info_data {
    uint32_t window_id;
    int32_t x, y;
    uint32_t width, height;
    uint8_t visible;
    float opacity;
    float scale_x, scale_y;
    float rotation;
    int32_t layer;
    uint32_t parent_id;
    uint32_t state; // bitfield: 1=minimized, 2=maximized, 4=fullscreen, 8=decorated
    uint32_t focused; // boolean
    uint32_t pid; // Process ID of owning application
    char process_name[255]; // Name of owning process
};*/
static int handle_query_window_info(struct IPCServer *ipc_server, struct IPCClient *client,
                                const struct icm_msg_query_window_info *msg) {
    // For ICM client windows (buffers), we can return info
    struct BufferEntry *buffer = ipc_buffer_get(ipc_server, msg->window_id);
    if (buffer) {
        struct icm_msg_window_info_data response = {
            .window_id = msg->window_id,
            .x = buffer->x,
            .y = buffer->y,
            .width = buffer->width,
            .height = buffer->height,
            .visible = buffer->visible,
            .opacity = buffer->opacity,
            .scale_x = buffer->scale_x,
            .scale_y = buffer->scale_y,
            .rotation = buffer->rotation,
            .layer = 0, // Default layer
            .parent_id = 0, // Root parent
            .state = buffer->minimized ? 1 : 0,
            .focused = buffer->focused,
            .pid = 0,
        };
        strncpy(response.process_name, "ICM Buffer", sizeof(response.process_name) - 1);
        send_event_to_client(client, ICM_MSG_WINDOW_INFO_DATA, &response, sizeof(response));
        return 0;
    }

    // Try to find as a View (xdg_toplevel window like Qt)
    struct Server *server = wl_container_of(ipc_server, server, ipc_server);
    struct View *view;
    wl_list_for_each(view, &server->views, link) {
        if (view->window_id == msg->window_id) {
            struct icm_msg_window_info_data response = {
                .window_id = msg->window_id,
                .x = view->x,
                .y = view->y,
                .width = view->xdg_surface->geometry.width > 0 ? view->xdg_surface->geometry.width : 400,
                .height = view->xdg_surface->geometry.height > 0 ? view->xdg_surface->geometry.height : 300,
                .visible = view->mapped,
                .opacity = view->opacity,
                .scale_x = view->scale_x,
                .scale_y = view->scale_y,
                .rotation = view->rotation,
                .layer = 2, // NORMAL layer
                .parent_id = 0,
                .state = 0,
                .focused = view->mapped && server->grabbed_view == view,
                .pid = 0,
            };
            
            // Copy title from xdg_toplevel
            if (view->xdg_surface && view->xdg_surface->toplevel && view->xdg_surface->toplevel->title) {
                strncpy(response.process_name, view->xdg_surface->toplevel->title, sizeof(response.process_name) - 1);
            } else {
                strncpy(response.process_name, "Untitled", sizeof(response.process_name) - 1);
            }
            response.process_name[sizeof(response.process_name) - 1] = '\0';
            
            send_event_to_client(client, ICM_MSG_WINDOW_INFO_DATA, &response, sizeof(response));
            fprintf(stderr, "Query window %u info: title='%s', pos=(%d,%d), size=%ux%u\n", 
                    msg->window_id, response.process_name, response.x, response.y, response.width, response.height);
            return 0;
        }
    }

    // Return error if window not found
    fprintf(stderr, "Window not found: %u\n", msg->window_id);
    return -1;
}

static int handle_query_toplevel_windows(struct IPCServer *ipc_server, struct IPCClient *client,
                                         const struct icm_msg_query_toplevel_windows *msg) {
    struct Server *server = wl_container_of(ipc_server, server, ipc_server);
    
    // Count visible toplevels
    uint32_t count = 0;
    struct View *view;
    wl_list_for_each(view, &server->views, link) {
        if (msg->flags == 0 || (msg->flags == 1 && view->mapped)) {
            count++;
        }
    }
    
    // Allocate response buffer
    size_t response_size = sizeof(struct icm_msg_toplevel_windows_data) + 
                          count * sizeof(struct icm_msg_toplevel_window_entry);
    uint8_t *response_buf = malloc(response_size);
    if (!response_buf) {
        return -1;
    }
    
    struct icm_msg_toplevel_windows_data *response = (struct icm_msg_toplevel_windows_data *)response_buf;
    response->num_windows = count;
    
    struct icm_msg_toplevel_window_entry *entries = 
        (struct icm_msg_toplevel_window_entry *)(response_buf + sizeof(struct icm_msg_toplevel_windows_data));
    
    // Fill in window data
    uint32_t idx = 0;
    wl_list_for_each(view, &server->views, link) {
        if (msg->flags == 0 || (msg->flags == 1 && view->mapped)) {
            entries[idx].window_id = view->window_id;
            entries[idx].x = view->x;
            entries[idx].y = view->y;
            entries[idx].width = view->xdg_surface->geometry.width > 0 ? view->xdg_surface->geometry.width : 400;
            entries[idx].height = view->xdg_surface->geometry.height > 0 ? view->xdg_surface->geometry.height : 300;
            entries[idx].visible = view->mapped;
            entries[idx].focused = (server->grabbed_view == view);
            entries[idx].state = 0;
            
            // Copy title
            if (view->xdg_surface && view->xdg_surface->toplevel && view->xdg_surface->toplevel->title) {
                strncpy(entries[idx].title, view->xdg_surface->toplevel->title, sizeof(entries[idx].title) - 1);
            } else {
                strncpy(entries[idx].title, "Untitled", sizeof(entries[idx].title) - 1);
            }
            entries[idx].title[sizeof(entries[idx].title) - 1] = '\0';
            
            // Copy app_id
            if (view->xdg_surface && view->xdg_surface->toplevel && view->xdg_surface->toplevel->app_id) {
                strncpy(entries[idx].app_id, view->xdg_surface->toplevel->app_id, sizeof(entries[idx].app_id) - 1);
            } else {
                strncpy(entries[idx].app_id, "", sizeof(entries[idx].app_id) - 1);
            }
            entries[idx].app_id[sizeof(entries[idx].app_id) - 1] = '\0';
            
            idx++;
        }
    }
    
    send_event_to_client(client, ICM_MSG_TOPLEVEL_WINDOWS_DATA, response_buf, response_size);
    free(response_buf);
    
    fprintf(stderr, "Query toplevel windows: found %u windows\n", count);
    return 0;
}

static int handle_subscribe_window_events(struct IPCServer *ipc_server, struct IPCClient *client,
                                          const struct icm_msg_subscribe_window_events *msg) {
    client->window_event_mask |= msg->event_mask;
    fprintf(stderr, "Client subscribed to window events: mask=0x%x\n", client->window_event_mask);
    return 0;
}

static int handle_unsubscribe_window_events(struct IPCServer *ipc_server, struct IPCClient *client,
                                            const struct icm_msg_unsubscribe_window_events *msg) {
    client->window_event_mask &= ~msg->event_mask;
    fprintf(stderr, "Client unsubscribed from window events: mask=0x%x\n", client->window_event_mask);
    return 0;
}

static int handle_set_window_decorations(struct IPCServer *ipc_server, struct IPCClient *client,
                                         const struct icm_msg_set_window_decorations *msg) {
    if (msg->server_side) {
        // Enable server-side decorations
        ipc_server->decoration_enabled = 1;
        ipc_server->decoration_title_height = msg->title_height;
        ipc_server->decoration_border_width = msg->border_width;
        ipc_server->decoration_color_focus = msg->color_focused;
        ipc_server->decoration_color_unfocus = msg->color_unfocused;
        
        fprintf(stderr, "Enabled server-side decorations: title_height=%u, border_width=%u\n",
                msg->title_height, msg->border_width);
    } else {
        // Disable server-side decorations (client will handle)
        ipc_server->decoration_enabled = 0;
        fprintf(stderr, "Disabled server-side decorations for window %u (client-side)\n", msg->window_id);
    }
    
    schedule_frame_update(ipc_server);
    return 0;
}

static int handle_request_window_decorations(struct IPCServer *ipc_server, struct IPCClient *client,
                                             const struct icm_msg_request_window_decorations *msg) {
    // Send current decoration settings
    struct icm_msg_set_window_decorations response = {
        .window_id = msg->window_id,
        .server_side = ipc_server->decoration_enabled,
        .title_height = ipc_server->decoration_title_height,
        .border_width = ipc_server->decoration_border_width,
        .color_focused = ipc_server->decoration_color_focus,
        .color_unfocused = ipc_server->decoration_color_unfocus,
    };
    
    send_event_to_client(client, ICM_MSG_SET_WINDOW_DECORATIONS, &response, sizeof(response));
    return 0;
}

static int handle_launch_app(struct IPCServer *ipc_server, struct IPCClient *client,
                             const struct icm_msg_launch_app *msg) {
    if (msg->command_len == 0 || msg->command[0] == '\0') {
        return -1;
    }
    
    // Fork and exec the command
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        setsid();
        execl("/bin/sh", "sh", "-c", msg->command, NULL);
        _exit(1);
    } else if (pid < 0) {
        perror("fork");
        return -1;
    }
    
    // Parent continues
    return 0;
}

/* Main message dispatcher */
static int process_message(struct IPCServer *ipc_server, struct IPCClient *client,
                           struct icm_ipc_header *header, uint8_t *payload,
                           const int *fds, int num_fds) {
    int ret = 0;

    switch (header->type) {
    case ICM_MSG_CREATE_BUFFER: {
        struct icm_msg_create_buffer *msg = (struct icm_msg_create_buffer *)payload;
        ret = handle_create_buffer(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_DESTROY_BUFFER: {
        struct icm_msg_destroy_buffer *msg = (struct icm_msg_destroy_buffer *)payload;
        ret = handle_destroy_buffer(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_IMPORT_DMABUF: {
        struct icm_msg_import_dmabuf *msg = (struct icm_msg_import_dmabuf *)payload;
        ret = handle_import_dmabuf(ipc_server, client, msg, fds, num_fds);
        break;
    }
    case ICM_MSG_DRAW_RECT: {
        struct icm_msg_draw_rect *msg = (struct icm_msg_draw_rect *)payload;
        ret = handle_draw_rect(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_DRAW_LINE: {
        struct icm_msg_draw_line *msg = (struct icm_msg_draw_line *)payload;
        ret = handle_draw_line(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_DRAW_CIRCLE: {
        struct icm_msg_draw_circle *msg = (struct icm_msg_draw_circle *)payload;
        ret = handle_draw_circle(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_DRAW_POLYGON: {
        ret = handle_draw_polygon(ipc_server, client, payload, header->length - sizeof(struct icm_ipc_header));
        break;
    }
    case ICM_MSG_BATCH_BEGIN: {
        struct icm_msg_batch_begin *msg = (struct icm_msg_batch_begin *)payload;
        client->batch_id = msg->batch_id;
        client->batching = 1;
        break;
    }
    case ICM_MSG_BATCH_END: {
        struct icm_msg_batch_end *msg = (struct icm_msg_batch_end *)payload;
        if (msg->batch_id == client->batch_id) {
            client->batching = 0;
        }
        break;
    }
    case ICM_MSG_EXPORT_SURFACE: {
        struct icm_msg_export_surface *msg = (struct icm_msg_export_surface *)payload;
        ret = handle_export_surface(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_IMPORT_SURFACE: {
        struct icm_msg_import_surface *msg = (struct icm_msg_import_surface *)payload;
        ret = handle_import_surface(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_REGISTER_POINTER_EVENT: {
        struct icm_msg_register_pointer_event *msg = (struct icm_msg_register_pointer_event *)payload;
        ret = handle_register_pointer_event(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_REGISTER_KEYBOARD_EVENT: {
        struct icm_msg_register_keyboard_event *msg = (struct icm_msg_register_keyboard_event *)payload;
        ret = handle_register_keyboard_event(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_QUERY_CAPTURE_MOUSE: {
        struct icm_msg_query_capture_mouse *msg = (struct icm_msg_query_capture_mouse *)payload;
        ret = handle_query_capture_mouse(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_QUERY_CAPTURE_KEYBOARD: {
        struct icm_msg_query_capture_keyboard *msg = (struct icm_msg_query_capture_keyboard *)payload;
        ret = handle_query_capture_keyboard(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_UPLOAD_IMAGE: {
        struct icm_msg_upload_image *msg = (struct icm_msg_upload_image *)payload;
        ret = handle_upload_image(ipc_server, client, msg, header->length - sizeof(struct icm_ipc_header));
        break;
    }
    case ICM_MSG_DESTROY_IMAGE: {
        struct icm_msg_destroy_image *msg = (struct icm_msg_destroy_image *)payload;
        ret = handle_destroy_image(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_DRAW_UPLOADED_IMAGE: {
        struct icm_msg_draw_uploaded_image *msg = (struct icm_msg_draw_uploaded_image *)payload;
        ret = handle_draw_uploaded_image(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_DRAW_TEXT: {
        struct icm_msg_draw_text *msg = (struct icm_msg_draw_text *)payload;
        ret = handle_draw_text(ipc_server, client, msg, header->length - sizeof(struct icm_ipc_header));
        break;
    }
    case ICM_MSG_SET_WINDOW_VISIBLE: {
        struct icm_msg_set_window_visible *msg = (struct icm_msg_set_window_visible *)payload;
        ret = handle_set_window_visible(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_REGISTER_KEYBIND: {
        struct icm_msg_register_keybind *msg = (struct icm_msg_register_keybind *)payload;
        ret = handle_register_keybind(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_UNREGISTER_KEYBIND: {
        struct icm_msg_unregister_keybind *msg = (struct icm_msg_unregister_keybind *)payload;
        ret = handle_unregister_keybind(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_REGISTER_CLICK_REGION: {
        struct icm_msg_register_click_region *msg = (struct icm_msg_register_click_region *)payload;
        ret = handle_register_click_region(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_UNREGISTER_CLICK_REGION: {
        struct icm_msg_unregister_click_region *msg = (struct icm_msg_unregister_click_region *)payload;
        ret = handle_unregister_click_region(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_REQUEST_SCREEN_COPY: {
        struct icm_msg_request_screen_copy *msg = (struct icm_msg_request_screen_copy *)payload;
        ret = handle_request_screen_copy(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_REGISTER_GLOBAL_POINTER_EVENT: {
        ret = handle_register_global_pointer_event(ipc_server, client);
        break;
    }
    case ICM_MSG_REGISTER_GLOBAL_KEYBOARD_EVENT: {
        ret = handle_register_global_keyboard_event(ipc_server, client);
        break;
    }
    case ICM_MSG_REGISTER_GLOBAL_CAPTURE_MOUSE: {
        ret = handle_register_global_capture_mouse(ipc_server, client);
        break;
    }
    case ICM_MSG_REGISTER_GLOBAL_CAPTURE_KEYBOARD: {
        ret = handle_register_global_capture_keyboard(ipc_server, client);
        break;
    }
    case ICM_MSG_UNREGISTER_GLOBAL_CAPTURE_KEYBOARD: {
        ret = handle_unregister_global_capture_keyboard(ipc_server, client);
        break;
    }
    case ICM_MSG_UNREGISTER_GLOBAL_CAPTURE_MOUSE: {
        ret = handle_unregister_global_capture_mouse(ipc_server, client);
        break;
    }
    case ICM_MSG_SET_WINDOW_POSITION: {
        struct icm_msg_set_window_position *msg = (struct icm_msg_set_window_position *)payload;
        ret = handle_set_window_position(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_SET_WINDOW_SIZE: {
        struct icm_msg_set_window_size *msg = (struct icm_msg_set_window_size *)payload;
        ret = handle_set_window_size(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_SET_WINDOW_OPACITY: {
        struct icm_msg_set_window_opacity *msg = (struct icm_msg_set_window_opacity *)payload;
        ret = handle_set_window_opacity(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_SET_WINDOW_BLUR: {
        struct icm_msg_set_window_blur *msg = (struct icm_msg_set_window_blur *)payload;
        ret = handle_set_window_blur(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_SET_SCREEN_EFFECT: {
        struct icm_msg_set_screen_effect *msg = (struct icm_msg_set_screen_effect *)payload;
        ret = handle_set_screen_effect(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_SET_WINDOW_EFFECT: {
        struct icm_msg_set_window_effect *msg = (struct icm_msg_set_window_effect *)payload;
        ret = handle_set_window_effect(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_SET_WINDOW_TRANSFORM: {
        struct icm_msg_set_window_transform *msg = (struct icm_msg_set_window_transform *)payload;
        ret = handle_set_window_transform(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_QUERY_WINDOW_POSITION: {
        fprintf(stderr, "Processing QUERY_WINDOW_POSITION message\n");
        struct icm_msg_query_window_position *msg = (struct icm_msg_query_window_position *)payload;
        ret = handle_query_window_position(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_QUERY_WINDOW_SIZE: {
        struct icm_msg_query_window_size *msg = (struct icm_msg_query_window_size *)payload;
        ret = handle_query_window_size(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_QUERY_WINDOW_ATTRIBUTES: {
        struct icm_msg_query_window_attributes *msg = (struct icm_msg_query_window_attributes *)payload;
        ret = handle_query_window_attributes(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_SET_WINDOW_LAYER: {
        struct icm_msg_set_window_layer *msg = (struct icm_msg_set_window_layer *)payload;
        ret = handle_set_window_layer(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_RAISE_WINDOW: {
        struct icm_msg_raise_window *msg = (struct icm_msg_raise_window *)payload;
        ret = handle_raise_window(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_LOWER_WINDOW: {
        struct icm_msg_lower_window *msg = (struct icm_msg_lower_window *)payload;
        ret = handle_lower_window(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_SET_WINDOW_PARENT: {
        struct icm_msg_set_window_parent *msg = (struct icm_msg_set_window_parent *)payload;
        ret = handle_set_window_parent(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_SET_WINDOW_TRANSFORM_3D: {
        struct icm_msg_set_window_transform_3d *msg = (struct icm_msg_set_window_transform_3d *)payload;
        ret = handle_set_window_transform_3d(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_SET_WINDOW_MATRIX: {
        struct icm_msg_set_window_matrix *msg = (struct icm_msg_set_window_matrix *)payload;
        ret = handle_set_window_matrix(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_SET_WINDOW_STATE: {
        struct icm_msg_set_window_state *msg = (struct icm_msg_set_window_state *)payload;
        ret = handle_set_window_state(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_FOCUS_WINDOW: {
        struct icm_msg_focus_window *msg = (struct icm_msg_focus_window *)payload;
        ret = handle_focus_window(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_BLUR_WINDOW: {
        struct icm_msg_blur_window *msg = (struct icm_msg_blur_window *)payload;
        ret = handle_blur_window(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_ANIMATE_WINDOW: {
        struct icm_msg_animate_window *msg = (struct icm_msg_animate_window *)payload;
        ret = handle_animate_window(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_STOP_ANIMATION: {
        struct icm_msg_stop_animation *msg = (struct icm_msg_stop_animation *)payload;
        ret = handle_stop_animation(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_QUERY_WINDOW_LAYER: {
        struct icm_msg_query_window_layer *msg = (struct icm_msg_query_window_layer *)payload;
        ret = handle_query_window_layer(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_QUERY_WINDOW_STATE: {
        struct icm_msg_query_window_state *msg = (struct icm_msg_query_window_state *)payload;
        ret = handle_query_window_state(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_QUERY_SCREEN_DIMENSIONS: {
        struct icm_msg_query_screen_dimensions *msg = (struct icm_msg_query_screen_dimensions *)payload;
        ret = handle_query_screen_dimensions(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_QUERY_MONITORS: {
        struct icm_msg_query_monitors *msg = (struct icm_msg_query_monitors *)payload;
        ret = handle_query_monitors(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_QUERY_WINDOW_INFO: {
        struct icm_msg_query_window_info *msg = (struct icm_msg_query_window_info *)payload;
        ret = handle_query_window_info(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_SET_WINDOW_MESH_TRANSFORM: {
        struct icm_msg_set_window_mesh_transform *msg = (struct icm_msg_set_window_mesh_transform *)payload;
        ret = handle_set_window_mesh_transform(ipc_server, client, msg, (const uint8_t *)payload, header->length - 16);
        break;
    }
    case ICM_MSG_CLEAR_WINDOW_MESH_TRANSFORM: {
        struct icm_msg_clear_window_mesh_transform *msg = (struct icm_msg_clear_window_mesh_transform *)payload;
        ret = handle_clear_window_mesh_transform(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_UPDATE_WINDOW_MESH_VERTICES: {
        struct icm_msg_update_window_mesh_vertices *msg = (struct icm_msg_update_window_mesh_vertices *)payload;
        ret = handle_update_window_mesh_vertices(ipc_server, client, msg, (const uint8_t *)payload, header->length - 16);
        break;
    }
    case ICM_MSG_QUERY_TOPLEVEL_WINDOWS: {
        struct icm_msg_query_toplevel_windows *msg = (struct icm_msg_query_toplevel_windows *)payload;
        ret = handle_query_toplevel_windows(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_SUBSCRIBE_WINDOW_EVENTS: {
        struct icm_msg_subscribe_window_events *msg = (struct icm_msg_subscribe_window_events *)payload;
        ret = handle_subscribe_window_events(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_UNSUBSCRIBE_WINDOW_EVENTS: {
        struct icm_msg_unsubscribe_window_events *msg = (struct icm_msg_unsubscribe_window_events *)payload;
        ret = handle_unsubscribe_window_events(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_SET_WINDOW_DECORATIONS: {
        struct icm_msg_set_window_decorations *msg = (struct icm_msg_set_window_decorations *)payload;
        ret = handle_set_window_decorations(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_REQUEST_WINDOW_DECORATIONS: {
        struct icm_msg_request_window_decorations *msg = (struct icm_msg_request_window_decorations *)payload;
        ret = handle_request_window_decorations(ipc_server, client, msg);
        break;
    }
    case ICM_MSG_LAUNCH_APP: {
        struct icm_msg_launch_app *msg = (struct icm_msg_launch_app *)payload;
        ret = handle_launch_app(ipc_server, client, msg);
        break;
    }
    default:
        if (header->type == 0) {
            fprintf(stderr, "Warning: Received null message type (possibly buffer sync issue)\n");
        } else {
            fprintf(stderr, "Warning: Unknown message type: %u (valid range: 1-77)\n", header->type);
        }
        break;
    }

    return ret;
}

/* Client I/O handler */
int ipc_server_handle_client(int fd, uint32_t mask, void *data) {
    struct IPCClient *client = (struct IPCClient *)data;
    struct Server *server = client->server;
    struct IPCServer *ipc_server = &server->ipc_server;

    if (mask & WL_EVENT_READABLE) {
        int fds[ICM_MAX_FDS_PER_MSG];
        int num_fds = 0;

        ssize_t n = recv_with_fds(client->socket_fd,
                                   client->read_buffer + client->read_pos,
                                   sizeof(client->read_buffer) - client->read_pos,
                                   fds, &num_fds, ICM_MAX_FDS_PER_MSG);

        if (n <= 0) {
            /* Client disconnected or error */
            wl_list_remove(&client->link);

            /* Clean up keybinds */
            struct KeybindEntry *kb, *kb_tmp;
            wl_list_for_each_safe(kb, kb_tmp, &ipc_server->keybinds, link) {
                if (kb->client == client) {
                    wl_list_remove(&kb->link);
                    free(kb);
                }
            }

            /* Clean up click regions */
            struct ClickRegion *cr, *cr_tmp;
            wl_list_for_each_safe(cr, cr_tmp, &ipc_server->click_regions, link) {
                if (cr->client == client) {
                    wl_list_remove(&cr->link);
                    free(cr);
                }
            }

            /* Clean up screen copy requests */
            struct ScreenCopyRequest *scr, *scr_tmp;
            wl_list_for_each_safe(scr, scr_tmp, &ipc_server->screen_copy_requests, link) {
                if (scr->client == client) {
                    wl_list_remove(&scr->link);
                    free(scr);
                }
            }

            if (client->event_source) {
                wl_event_source_remove(client->event_source);
            }
            close(client->socket_fd);
            free(client);
            return 0;
        }

        client->read_pos += n;

        /* Process complete messages */
        while (client->read_pos >= sizeof(struct icm_ipc_header)) {
            /* Read header in little-endian format */
            uint8_t *buf = client->read_buffer;
            uint32_t msg_length = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
            uint16_t msg_type = buf[4] | (buf[5] << 8);
            uint16_t msg_flags = buf[6] | (buf[7] << 8);
            uint32_t msg_sequence = buf[8] | (buf[9] << 8) | (buf[10] << 16) | (buf[11] << 24);
            int32_t msg_num_fds = buf[12] | (buf[13] << 8) | (buf[14] << 16) | (buf[15] << 24);

            fprintf(stderr, "Received message type %u, length %u\n", msg_type, msg_length);

            /* Validate header length */
            if (msg_length < sizeof(struct icm_ipc_header) || msg_length > 65536) {
                fprintf(stderr, "Invalid message length: %u (expected 16-%u)\n", 
                        msg_length, 65536);
                /* Skip this byte and try to resync */
                memmove(client->read_buffer, client->read_buffer + 1, client->read_pos - 1);
                client->read_pos--;
                continue;
            }

            if (client->read_pos < msg_length) {
                break;  /* Incomplete message */
            }

            /* Validate message type */
            if (msg_type < 1 || msg_type > 100) {
                fprintf(stderr, "Invalid message type: %u\n", msg_type);
                /* Skip this message and continue */
                memmove(client->read_buffer,
                       client->read_buffer + msg_length,
                       client->read_pos - msg_length);
                client->read_pos -= msg_length;
                continue;
            }

            uint8_t *payload = client->read_buffer + sizeof(struct icm_ipc_header);
            uint32_t payload_size = msg_length - sizeof(struct icm_ipc_header);

            /* Create a temporary header struct for the handler */
            struct icm_ipc_header header = {
                .length = msg_length,
                .type = msg_type,
                .flags = msg_flags,
                .sequence = msg_sequence,
                .num_fds = msg_num_fds
            };

            process_message(ipc_server, client, &header, payload, fds, num_fds);

            /* Move remaining data to front */
            memmove(client->read_buffer,
                   client->read_buffer + msg_length,
                   client->read_pos - msg_length);
            client->read_pos -= msg_length;
        }
    }

    return 0;
}

/* New connection handler */
static int ipc_handle_new_connection(int fd, uint32_t mask, void *data) {
    struct IPCServer *ipc_server = (struct IPCServer *)data;

    int client_fd = accept(ipc_server->socket_fd, NULL, NULL);
    if (client_fd < 0) {
        fprintf(stderr, "accept failed: %s\n", strerror(errno));
        return 0;
    }

    fcntl(client_fd, F_SETFL, O_NONBLOCK);

    struct IPCClient *client = calloc(1, sizeof(*client));
    if (!client) {
        close(client_fd);
        return 0;
    }

    client->socket_fd = client_fd;
    client->server = ipc_server->server;
    client->read_pos = 0;
    client->batching = 0;
    client->registered_pointer = 0;
    client->registered_keyboard = 0;
    client->event_window_id = 0;

    client->event_source = wl_event_loop_add_fd(
        wl_display_get_event_loop(ipc_server->server->wl_display),
        client_fd, WL_EVENT_READABLE, ipc_server_handle_client, client);

    wl_list_insert(&ipc_server->clients, &client->link);

    fprintf(stderr, "New IPC client connected (fd=%d)\n", client_fd);
    return 0;
}

int ipc_server_init(struct IPCServer *ipc_server, struct Server *server,
                    const char *socket_path) {
    ipc_server->server = server;
    ipc_server->next_buffer_id = 1;
    ipc_server->next_surface_id = 1;
    ipc_server->next_image_id = 1;
    ipc_server->next_keybind_id = 1;
    ipc_server->next_region_id = 1;
    ipc_server->next_window_id = 1;
    ipc_server->screen_effect_equation[0] = '\0';
    ipc_server->screen_effect_enabled = 0;
    ipc_server->screen_effect_buffer = NULL;
    ipc_server->screen_effect_dirty = 0;
    
    /* Initialize decoration defaults */
    ipc_server->decoration_border_width = 2;         /* 2px borders */
    ipc_server->decoration_title_height = 30;        /* 30px title bar */
    ipc_server->decoration_color_focus = 0x3366FFFF; /* Blue: RGB(0x33, 0x66, 0xFF), A=0xFF */
    ipc_server->decoration_color_unfocus = 0x888888FF; /* Gray: RGB(0x88, 0x88, 0x88), A=0xFF */
    ipc_server->decoration_enabled = 1;              /* Enable decorations by default */
    
    wl_list_init(&ipc_server->clients);
    wl_list_init(&ipc_server->buffers);
    wl_list_init(&ipc_server->surfaces);
    wl_list_init(&ipc_server->images);
    wl_list_init(&ipc_server->keybinds);
    wl_list_init(&ipc_server->click_regions);
    wl_list_init(&ipc_server->screen_copy_requests);

    /* Create Unix domain socket */
    ipc_server->socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ipc_server->socket_fd < 0) {
        fprintf(stderr, "socket failed: %s\n", strerror(errno));
        return -1;
    }

    fcntl(ipc_server->socket_fd, F_SETFL, O_NONBLOCK);

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    /* Remove existing socket file */
    unlink(socket_path);

    if (bind(ipc_server->socket_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "bind failed: %s\n", strerror(errno));
        close(ipc_server->socket_fd);
        return -1;
    }

    if (listen(ipc_server->socket_fd, 8) < 0) {
        fprintf(stderr, "listen failed: %s\n", strerror(errno));
        close(ipc_server->socket_fd);
        return -1;
    }

    ipc_server->event_source = wl_event_loop_add_fd(
        wl_display_get_event_loop(server->wl_display),
        ipc_server->socket_fd, WL_EVENT_READABLE,
        ipc_handle_new_connection, ipc_server);

    fprintf(stderr, "IPC server listening on %s\n", socket_path);
    return 0;
}

/* Destroy IPC server */
void ipc_server_destroy(struct IPCServer *ipc_server) {
    /* Close all client connections */
    struct IPCClient *client, *tmp_client;
    wl_list_for_each_safe(client, tmp_client, &ipc_server->clients, link) {
        wl_list_remove(&client->link);
        close(client->socket_fd);
        free(client);
    }

    /* Cleanup buffers */
    struct BufferEntry *buffer, *tmp_buffer;
    wl_list_for_each_safe(buffer, tmp_buffer, &ipc_server->buffers, link) {
        ipc_buffer_destroy(ipc_server, buffer->buffer_id);
    }

    /* Cleanup exported surfaces */
    struct ExportedSurface *surface, *tmp_surface;
    wl_list_for_each_safe(surface, tmp_surface, &ipc_server->surfaces, link) {
        wl_list_remove(&surface->link);
        if (surface->buffer) {
            ipc_buffer_destroy(ipc_server, surface->buffer->buffer_id);
        }
        free(surface);
    }

    /* Cleanup images */
    struct ImageEntry *image, *tmp_image;
    wl_list_for_each_safe(image, tmp_image, &ipc_server->images, link) {
        ipc_image_destroy(ipc_server, image->image_id);
    }

    /* Close socket */
    if (ipc_server->event_source) {
        wl_event_source_remove(ipc_server->event_source);
    }
    if (ipc_server->socket_fd >= 0) {
        close(ipc_server->socket_fd);
    }
}