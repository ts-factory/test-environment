/** @file 
 * @brief CWMP data common methods implementation
 * 
 * CWMP data exchange common methods, useful for transfer CWMP message
 * structures, declared in cwmp_soapStub.h, between processes with 
 * different address spaces.
 *
 *
 * Copyright (C) 2010 Test Environment authors (see file AUTHORS
 * in the root directory of the distribution). 
 *
 * Test Environment is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version. 
 *
 * Test Environment is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details. 
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 *
 * @author Konstantin Abramenko <Konstantin.Abramenko@oktetlabs.ru>
 *
 * $Id$
*/


#include "te_config.h"
#include "te_defs.h"
#include "logger_api.h"
#include "cwmp_data.h"
#include "acse_soapH.h"
#include "cwmp_utils.h"

#include <string.h>
#include <strings.h>

int va_end_list_var = 10;
void const * const va_end_list_ptr = &va_end_list_ptr;

#define CWMP_VAL_ARR_QUANTUM 8

static inline te_errno
cwmp_str_array_add_va(string_array_t *a,
                      const char *b_name,
                      const char *first_name,
                      va_list ap)
{
    size_t      real_arr_len = a->size, i = a->size;
    size_t      b_len, n_len;
    const char *v_name = first_name;
    char      **array = a->items;

    b_len = strlen(b_name);

    do {
        n_len = (NULL != v_name ? strlen(v_name) : 0);

        if (real_arr_len <= i)
        {
            real_arr_len += CWMP_VAL_ARR_QUANTUM;
            a->items = array = realloc(array,
                                       real_arr_len * sizeof(char*));
        }
        if (NULL == (array[i] = malloc(b_len + n_len + 1)))
            return TE_ENOMEM;

        strcpy(array[i], b_name);
        if (v_name != NULL)
            strcpy(array[i] + b_len, v_name);

        a->size = (++i);
        v_name = va_arg(ap, const char *);
    } while (VA_END_LIST != v_name);

    return 0;
}

/* see description in cwmp_utils.h */
string_array_t *
cwmp_str_array_copy(string_array_t *src)
{
    unsigned i;
    string_array_t *res = malloc(sizeof(*res));
    if (NULL == src)
        return NULL;
    res->size = src->size;
    if (NULL == (res->items = calloc(src->size, sizeof(char *))))
        return NULL;
    for (i = 0; i < src->size; i++)
        if (src->items[i])
            if (NULL == (res->items[i] = strdup(src->items[i])))
                return NULL;
    return res;
}

/* see description in cwmp_utils.h */
string_array_t *
cwmp_str_array_alloc(const char *b_name, const char *first_name, ...)
{
    va_list         ap;
    te_errno        rc; 
    string_array_t *ret;

    if (NULL == (ret = malloc(sizeof(*ret))))
        return NULL;
    ret->items = NULL;
    ret->size = 0;

    if (NULL == b_name || NULL == first_name)
        return ret;

    va_start(ap, first_name);
    rc = cwmp_str_array_add_va(ret, b_name, first_name, ap);
    va_end(ap);
    if (rc != 0)
    {
        WARN("%s(): alloc string array failed %r", __FUNCTION__, rc);
        cwmp_str_array_free(ret);
        return NULL;
    } 

    return ret;
}



/* see description in cwmp_utils.h */
te_errno
cwmp_str_array_add(string_array_t *a,
                   const char *b_name, const char *first_name, ...)
{
    va_list     ap;
    te_errno    rc;

    if (NULL == a || NULL == b_name || NULL == first_name)
        return TE_EINVAL;

    va_start(ap, first_name);
    rc = cwmp_str_array_add_va(a, b_name, first_name, ap);
    va_end(ap);

    return rc;
}


/* see description in cwmp_utils.h */
te_errno
cwmp_str_array_cat_tail(string_array_t *a, const char *suffix)
{
    size_t s_len;
    unsigned i;

    if (NULL == a || NULL == suffix)
        return TE_EINVAL;

    s_len = strlen(suffix);
    for (i = 0; i < a->size; i++)
    {
        size_t item_len;
        if (NULL == a->items[i])
            continue;
        item_len = strlen(a->items[i]);
        a->items[i] = realloc(a->items[i], item_len + s_len + 1);
        if (NULL == a->items[i]) 
            /* out of memory, it seems unnecessary to leave consistancy */
            return TE_ENOMEM;
        memcpy(a->items[i] + item_len, suffix, s_len + 1);
    }
    return 0;
}


/* see description in cwmp_utils.h */
void
cwmp_str_array_free(string_array_t *a)
{
    if (NULL == a)
        return;
    if (a->size > 0)
    {
        unsigned i;
        for (i = 0; i < a->size; i++)
            free(a->items[i]);
    }
    free(a->items);
    free(a);
}

/* */
#define STR_LOG_MAX 256

te_errno
cwmp_str_array_log(unsigned log_level, const char *intro, string_array_t *a)
{
    size_t log_buf_size = STR_LOG_MAX * (a->size + 1);
    char *log_buf = malloc(log_buf_size);
    char *s = log_buf;
    size_t p, total_p = 0, i;
    if (NULL == log_buf) return TE_ENOMEM;

    if (NULL == intro) intro = "CWMP_UTILS, array of string";
    p = snprintf(s, log_buf_size - total_p, "%s:\n", intro);
    s += p; total_p += p;

    for (i = 0; (i < a->size) && (total_p < log_buf_size); i++)
    {
        p = snprintf(s, log_buf_size - total_p, "   %s:\n", a->items[i]);
        s += p; total_p += p;
    }

    LGR_MESSAGE(log_level, TE_LGR_USER, log_buf);

    free(log_buf);

    return 0;
}

/* Internal common method */
static inline te_errno
cwmp_val_array_add_va(cwmp_values_array_t *a,
                      const char *base_name, const char *first_name,
                      va_list ap)
{
    size_t      real_arr_len = a->size, i = a->size;
    const char *v_name = first_name;
    size_t      b_len, v_len;

    cwmp_parameter_value_struct_t **array = a->items;

    b_len = strlen(base_name);

    VERB("add vals to val_array. b_len %d; first_name '%s'", 
         b_len, first_name);

    do {
        if (real_arr_len <= i)
        {
            real_arr_len += CWMP_VAL_ARR_QUANTUM;
            a->items = array = realloc(array,
                                     real_arr_len * sizeof(array[0]));
        }
        v_len = (NULL != v_name ? strlen(v_name) : 0);
        array[i] = malloc(sizeof(cwmp_parameter_value_struct_t));
        if (NULL == array[i])
            return TE_ENOMEM;
        array[i]->Name = malloc(b_len + v_len + 1);
        if (NULL == array[i]->Name)
            return TE_ENOMEM;

        memcpy(array[i]->Name, base_name, b_len);
        memcpy(array[i]->Name + b_len, v_name, v_len + 1);

        VERB("add val to val_array[%d]: v_len %d; sfx '%s', Name '%s'", 
             i, v_len, v_name, array[i]->Name);

        array[i]->__type = va_arg(ap, int);
        switch (array[i]->__type)
        {
#define SET_SOAP_TYPE(type_, arg_type_) \
            do { \
                type_ val = va_arg(ap, arg_type_ ); \
                array[i]->Value = malloc(sizeof(type_)); \
                *((type_ *)array[i]->Value) = val; \
            } while (0)

            case SOAP_TYPE_boolean:         
            case SOAP_TYPE_int:         
                SET_SOAP_TYPE(int, int);  break;
            case SOAP_TYPE_byte:
                SET_SOAP_TYPE(char, int); break;
            case SOAP_TYPE_unsignedInt:
                SET_SOAP_TYPE(uint32_t, int); break;
            case SOAP_TYPE_unsignedByte:
                SET_SOAP_TYPE(uint8_t, int); break;
            case SOAP_TYPE_time:
                SET_SOAP_TYPE(time_t, time_t); break;
#undef SET_SOAP_TYPE
            case SOAP_TYPE_string:
            case SOAP_TYPE_SOAP_ENC__base64:
                {
                    const char * val = va_arg(ap, const char *);
                    array[i]->Value = strdup(NULL != val ? val : "");
                }
                break;
            default:
                RING("%s(): unsupported type %d",
                     __FUNCTION__, array[i]->__type);
            return TE_EINVAL;
        }

        v_name = va_arg(ap, const char *);
        a->size = (++i);
    } while (VA_END_LIST != v_name);

    return 0;
}


cwmp_values_array_t *
cwmp_val_array_alloc(const char *b_name, const char *first_name, ...)
{
    va_list         ap;
    te_errno        rc; 

    cwmp_values_array_t *ret;

    if (NULL == b_name || NULL == first_name)
        return NULL;

    if (NULL == (ret = malloc(sizeof(*ret))))
        return NULL;
    ret->items = NULL;
    ret->size = 0;

    va_start(ap, first_name);
    rc = cwmp_val_array_add_va(ret, b_name, first_name, ap);
    va_end(ap);
    if (rc != 0)
    {
        WARN("%s(): alloc string array failed %r", __FUNCTION__, rc);
        cwmp_val_array_free(ret);
        return NULL;
    } 

    return ret;
}


te_errno
cwmp_val_array_add(cwmp_values_array_t *a,
                   const char *b_name, const char *first_name, ...)
{
    va_list     ap;
    te_errno    rc;

    if (NULL == a || NULL == b_name || NULL == first_name)
        return TE_EINVAL;

    va_start(ap, first_name);
    rc = cwmp_val_array_add_va(a, b_name, first_name, ap);
    va_end(ap);

    return rc;
}



/* see description in cwmp_utils.h */
void
cwmp_val_array_free(cwmp_values_array_t *a)
{
    if (NULL == a)
        return;
    if (a->size > 0)
    {
        unsigned i;
        for (i = 0; i < a->size; i++)
        {
            free(a->items[i]->Name);
            free(a->items[i]->Value);
            free(a->items[i]);
        }
    }
    free(a->items);
    free(a);
}



/* see description in cwmp_utils.h */
te_errno
cwmp_val_array_get_int(cwmp_values_array_t *a, const char *name, 
                       int *type, int *value)
{
    unsigned i;

    if (NULL == a || NULL == name || NULL == value)
        return TE_EINVAL;

    for (i = 0; i < a->size; i++)
    {
        char *suffix = rindex(a->items[i]->Name, '.');
        if (NULL == suffix)
            continue;
        if (strcmp(suffix + 1, name) == 0)
        {
            switch (a->items[i]->__type)
            {
                case SOAP_TYPE_xsd__boolean:         
                case SOAP_TYPE_int:         
                case SOAP_TYPE_unsignedInt:
                    *value = *((int *)a->items[i]->Value);
                    break;
                case SOAP_TYPE_byte:
                case SOAP_TYPE_unsignedByte:
                    *value = *((int8_t *)a->items[i]->Value);
                    break;
                    
                case SOAP_TYPE_time:
                case SOAP_TYPE_string:
                case SOAP_TYPE_SOAP_ENC__base64:
                    return TE_EBADTYPE;
            }
            if (NULL != type)
                *type = a->items[i]->__type;
            return 0;
        }
    }
    return TE_ENOENT;
}



#define VAL_LOG_MAX 512

te_errno
cwmp_val_array_log(unsigned log_level, const char *intro,
                   cwmp_values_array_t *a)
{
    size_t log_buf_size = VAL_LOG_MAX * (a->size + 1);
    char *log_buf = malloc(log_buf_size);
    char *s = log_buf;
    size_t p, total_p = 0, i;
    if (NULL == log_buf) return TE_ENOMEM;

    if (NULL == intro) intro = "CWMP_UTILS, array of values";
    p = snprintf(s, log_buf_size - total_p, "%s:\n    ", intro);
    s += p; total_p += p;

    for (i = 0; (i < a->size) && (total_p < log_buf_size); i++)
    {
        p = snprint_ParamValueStruct(s, log_buf_size-total_p, a->items[i]);
        s += p; total_p += p;
        p = snprintf(s, log_buf_size-total_p, "\n    ");
        s += p; total_p += p;
    }

    LGR_MESSAGE(log_level, TE_LGR_USER, log_buf);

    free(log_buf);

    return 0;
}

/* ================= OLD style API =========================== */

#if 0
/* see description in cwmp_utils.h */
_cwmp__GetParameterValues *
cwmp_get_values_alloc(const char *b_name, const char *first_name, ...)
{
    va_list     ap;
    te_errno    rc;
    string_array_t *array;
    _cwmp__GetParameterValues *ret;

    if (NULL == b_name || NULL == first_name)
        return NULL;
    array = malloc(sizeof(*array));
    array->items = NULL;
    array->size = 0;

    va_start(ap, first_name);
    rc = cwmp_str_array_add_va(array, b_name, first_name, ap);
    va_end(ap);
    if (rc != 0)
    {
        cwmp_str_array_free(array);
        return NULL;
    }

    ret = malloc(sizeof(*ret));

    ret->ParameterNames_ = malloc(sizeof(struct ParameterNames));
    ret->ParameterNames_->__size      = array->size;
    ret->ParameterNames_->__ptrstring = array->items;
    free(array);

    return ret;
} 

/* see description in cwmp_utils.h */
te_errno
cwmp_get_values_add(_cwmp__GetParameterValues *req,
                    const char *b_name,
                    const char *f_name, ...)
{
    ERROR("%s(): TODO", __FUNCTION__);
    return TE_EFAIL;
}


/* see description in cwmp_utils.h */
void
cwmp_get_values_free(_cwmp__GetParameterValues *req)
{
    ssize_t i;
    if (NULL == req)
        return;
    do {
        if (NULL == req->ParameterNames_)
            break;
        for (i = 0; i < req->ParameterNames_->__size; i++)
            free(req->ParameterNames_->__ptrstring[i]);
        free(req->ParameterNames_);
    } while (0);
    free(req);
}

/* see description in cwmp_utils.h */
/* see description in tapi_acse.h */
void
cwmp_get_values_resp_free(_cwmp__GetParameterValues *resp)
{
    if (NULL == resp)
        return;
    /* response is assumed to be obtained from this TAPI, with pointers,
        which are filled by cwmp_data_unpack_* methods, so there is only
        one block of allocated memory */
    free(resp);
    return;
}




/* see description in cwmp_utils.h */
_cwmp__SetParameterValues *
cwmp_set_values_alloc(const char *par_key,
                      const char *base_name, 
                      const char *first_name, ...)
{
    va_list     ap;
    size_t      real_arr_len = 0, i = 0;
    const char *v_name = first_name;
    size_t      b_len, v_len;

    struct cwmp__ParameterValueStruct **array = NULL;

    b_len = (NULL == base_name ? 0 : strlen(base_name));

    _cwmp__SetParameterValues *ret = malloc(sizeof(*ret));

    ret->ParameterKey = strdup(NULL != par_key ? par_key : "tapi acse");

    va_start(ap, first_name);
    do {
        if (real_arr_len <= i)
        {
            real_arr_len += CWMP_VAL_ARR_QUANTUM;
            array = realloc(array, real_arr_len * sizeof(array[0]));
        }
        v_len = (NULL != v_name ? strlen(v_name) : 0);
        array[i] = malloc(sizeof(struct cwmp__ParameterValueStruct));
        array[i]->Name = malloc(b_len + v_len + 1);
        memcpy(array[i]->Name, base_name, b_len);
        memcpy(array[i]->Name + b_len, v_name, v_len);

        array[i]->__type = va_arg(ap, int);
        switch (array[i]->__type)
        {
#define SET_SOAP_TYPE(type_, arg_type_) \
            do { \
                type_ val = va_arg(ap, arg_type_ ); \
                array[i]->Value = malloc(sizeof(type_)); \
                *((type_ *)array[i]->Value) = val; \
            } while (0)

            case SOAP_TYPE_xsd__boolean:         
            case SOAP_TYPE_int:         
                SET_SOAP_TYPE(int, int);  break;
            case SOAP_TYPE_byte:
                SET_SOAP_TYPE(char, int); break;
            case SOAP_TYPE_unsignedInt:
                SET_SOAP_TYPE(uint32_t, int); break;
            case SOAP_TYPE_unsignedByte:
                SET_SOAP_TYPE(uint8_t, int); break;
            case SOAP_TYPE_time:
                SET_SOAP_TYPE(time_t, time_t); break;

            case SOAP_TYPE_string:
            case SOAP_TYPE_SOAP_ENC__base64:
                {
                    const char * val = va_arg(ap, const char *);
                    array[i]->Value = strdup(NULL != val ? val : "");
                }
                break;
        }

        v_name = va_arg(ap, const char *);
        i++;
    } while (VA_END_LIST != v_name);
    va_end(ap);

    ret->ParameterList = malloc(sizeof(struct ParameterValueList));
    ret->ParameterList->__size = i;
    ret->ParameterList->__ptrParameterValueStruct = array;

    return ret;
}


void
cwmp_set_values_free(_cwmp__SetParameterValues *req)
{
    int i;
    if (NULL == req)
        return;
    do {
        free(req->ParameterKey);

        if (NULL == req->ParameterList)
            break;
        for (i = 0; i < req->ParameterList->__size; i++)
        {
            struct cwmp__ParameterValueStruct *par_val = 
                req->ParameterList->__ptrParameterValueStruct[i];
            free(par_val->Name);
            free(par_val->Value);
            free(par_val);
        }
        free(req->ParameterList);
    } while (0);
    free(req);
}
#endif

static inline const char *
soap_simple_type_string(int type)
{
    static char buf[10];
    switch (type)
    {
        case SOAP_TYPE_int:          return "SOAP_TYPE_int";
        case SOAP_TYPE_xsd__boolean: return "SOAP_TYPE_boolean";
        case SOAP_TYPE_byte:         return "SOAP_TYPE_byte";
        case SOAP_TYPE_string:       return "SOAP_TYPE_string";
        case SOAP_TYPE_unsignedInt:  return "SOAP_TYPE_unsignedInt";
        case SOAP_TYPE_unsignedByte: return "SOAP_TYPE_unsignedByte";
        case SOAP_TYPE_time:         return "SOAP_TYPE_time";
        case SOAP_TYPE_SOAP_ENC__base64: return "SOAP_TYPE_base64";
    }
    snprintf(buf, sizeof(buf), "<unknown: %d>", type);
    return buf;
}

size_t
snprint_ParamValueStruct(char *buf, size_t len, 
                         cwmp__ParameterValueStruct *p_v)
{
    size_t rest_len = len;
    size_t used;

    char *p = buf;
    void *v  = p_v->Value;
    int  type = p_v->__type;

    used = sprintf(p, "%s (type %s) = ", p_v->Name, 
            soap_simple_type_string(p_v->__type));
    p+= used; rest_len -= used;
    switch(type)
    {
        case SOAP_TYPE_string:
        case SOAP_TYPE_SOAP_ENC__base64:
            p+= snprintf(p, rest_len, "'%s'", (char *)v);
            break;
        case SOAP_TYPE_time:
            p+= snprintf(p, rest_len, "time %dsec", (int)(*((time_t *)v)));
            break;
        case SOAP_TYPE_byte:
            p+= snprintf(p, rest_len, "%d", (int)(*((char *)v)));
            break;
        case SOAP_TYPE_int:    
            p+= snprintf(p, rest_len, "%d", *((int *)v));
            break;
        case SOAP_TYPE_unsignedInt:
            p+= snprintf(p, rest_len, "%u", *((uint32_t *)v));
            break;
        case SOAP_TYPE_unsignedByte:
            p+= snprintf(p, rest_len, "%u", (uint32_t)(*((uint8_t *)v)));
            break;
        case SOAP_TYPE_xsd__boolean:
            p+= snprintf(p, rest_len, *((int *)v) ? "True" : "False");
            break;
    }
    return p - buf;
}

#define BUF_LOG_SIZE 32768
static char buf_log[BUF_LOG_SIZE];

void
tapi_acse_log_fault(_cwmp__Fault *f)
{
    char *p = buf_log;

    p += snprintf(p, BUF_LOG_SIZE - (p - buf_log), 
                  "CWMP Fault: %s (%s)", f->FaultCode, f->FaultString);
    if (f->__sizeSetParameterValuesFault > 0)
    {
        int i;
        p += snprintf(p, BUF_LOG_SIZE - (p - buf_log), 
                      "; Set details: ");
        for (i = 0; i < f->__sizeSetParameterValuesFault; i++)
        {
            struct _cwmp__Fault_SetParameterValuesFault *p_v_f = 
                                        &(f->SetParameterValuesFault[i]);
            p += snprintf(p, BUF_LOG_SIZE - (p - buf_log), 
                          "param[%d], name %s, fault %s(%s)",
                          i, p_v_f->ParameterName,
                          p_v_f->FaultCode,
                          p_v_f->FaultString);
        }
    }
    WARN(buf_log);
    buf_log[0] = '\0';
}


