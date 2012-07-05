
#include "nameser.h"

#define CHECK_CHANNEL(ch)                                                           \
    do {                                                                            \
        if (!ch->channel) {                                                         \
            PyErr_SetString(PyExc_AresError, "Channel has already been destroyed"); \
            return NULL;                                                            \
        }                                                                           \
    } while(0)                                                                      \


static Bool ares_lib_initialized = False;

static void
_ares_cleanup(void)
{
    if (ares_lib_initialized) {
        ares_library_cleanup();
    }
}


static PyObject* PyExc_AresError;


static PyTypeObject AresHostResultType;

static PyStructSequence_Field ares_host_result_fields[] = {
    {"name", ""},
    {"aliases", ""},
    {"addresses", ""},
    {NULL}
};

static PyStructSequence_Desc ares_host_result_desc = {
    "ares_host_result",
    NULL,
    ares_host_result_fields,
    3
};

static PyTypeObject AresNameinfoResultType;

static PyStructSequence_Field ares_nameinfo_result_fields[] = {
    {"node", ""},
    {"service", ""},
    {NULL}
};

static PyStructSequence_Desc ares_nameinfo_result_desc = {
    "ares_nameinfo_result",
    NULL,
    ares_nameinfo_result_fields,
    2
};

static PyTypeObject AresQueryMXResultType;

static PyStructSequence_Field ares_query_mx_result_fields[] = {
    {"host", ""},
    {"priority", ""},
    {NULL}
};

static PyStructSequence_Desc ares_query_mx_result_desc = {
    "ares_query_mx_result",
    NULL,
    ares_query_mx_result_fields,
    2
};

static PyTypeObject AresQuerySRVResultType;

static PyStructSequence_Field ares_query_srv_result_fields[] = {
    {"host", ""},
    {"port", ""},
    {"priority", ""},
    {"weight", ""},
    {NULL}
};

static PyStructSequence_Desc ares_query_srv_result_desc = {
    "ares_query_srv_result",
    NULL,
    ares_query_srv_result_fields,
    4
};

static PyTypeObject AresQueryNAPTRResultType;

static PyStructSequence_Field ares_query_naptr_result_fields[] = {
    {"order", ""},
    {"preference", ""},
    {"flags", ""},
    {"service", ""},
    {"regex", ""},
    {"replacement", ""},
    {NULL}
};

static PyStructSequence_Desc ares_query_naptr_result_desc = {
    "ares_query_naptr_result",
    NULL,
    ares_query_naptr_result_fields,
    6
};


static void
ares__sock_state_cb(void *data, ares_socket_t socket_fd, int readable, int writable)
{
    PyGILState_STATE gstate = PyGILState_Ensure();
    Channel *self;
    PyObject *result, *fd, *py_readable, *py_writable;

    self = (Channel *)data;
    ASSERT(self);
    /* Object could go out of scope in the callback, increase refcount to avoid it */
    Py_INCREF(self);

    fd = PyInt_FromLong((long)socket_fd);
    py_readable = PyBool_FromLong((long)readable); 
    py_writable = PyBool_FromLong((long)writable); 

    result = PyObject_CallFunctionObjArgs(self->sock_state_cb, fd, py_readable, py_writable, NULL);
    if (result == NULL) {
        PyErr_WriteUnraisable(self->sock_state_cb);
    }
    Py_XDECREF(result);
    Py_DECREF(fd);
    Py_DECREF(py_readable);
    Py_DECREF(py_writable);

    Py_DECREF(self);
    PyGILState_Release(gstate);
}


static void
query_a_cb(void *arg, int status,int timeouts, unsigned char *answer_buf, int answer_len)
{
    PyGILState_STATE gstate = PyGILState_Ensure();
    int parse_status;
    char ip[INET6_ADDRSTRLEN];
    char **ptr;
    struct hostent *hostent;
    PyObject *dns_result, *errorno, *tmp, *result, *callback;

    callback = (PyObject *)arg;
    ASSERT(callback);

    if (status != ARES_SUCCESS) {
        errorno = PyInt_FromLong((long)status);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    parse_status = ares_parse_a_reply(answer_buf, answer_len, &hostent, NULL, NULL);
    if (parse_status != ARES_SUCCESS) {
        errorno = PyInt_FromLong((long)parse_status);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    dns_result = PyList_New(0);
    if (!dns_result) {
        PyErr_NoMemory();
        PyErr_WriteUnraisable(Py_None);
        errorno = PyInt_FromLong((long)ARES_ENOMEM);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    for (ptr = hostent->h_addr_list; *ptr != NULL; ptr++) {
        uv_inet_ntop(hostent->h_addrtype, *ptr, ip, sizeof(ip));
        tmp = PYUVString_FromString(ip);
        if (tmp == NULL) {
            break;
        }
        PyList_Append(dns_result, tmp);
        Py_DECREF(tmp);
    }
    ares_free_hostent(hostent);
    errorno = Py_None;
    Py_INCREF(Py_None);

callback:
    result = PyObject_CallFunctionObjArgs(callback, dns_result, errorno, NULL);
    if (result == NULL) {
        PyErr_WriteUnraisable(callback);
    }
    Py_XDECREF(result);
    Py_DECREF(dns_result);
    Py_DECREF(errorno);

    Py_DECREF(callback);
    PyGILState_Release(gstate);
}


static void
query_aaaa_cb(void *arg, int status,int timeouts, unsigned char *answer_buf, int answer_len)
{
    PyGILState_STATE gstate = PyGILState_Ensure();
    int parse_status;
    char ip[INET6_ADDRSTRLEN];
    char **ptr;
    struct hostent *hostent;
    PyObject *dns_result, *errorno, *tmp, *result, *callback;

    callback = (PyObject *)arg;
    ASSERT(callback);

    if (status != ARES_SUCCESS) {
        errorno = PyInt_FromLong((long)status);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    parse_status = ares_parse_aaaa_reply(answer_buf, answer_len, &hostent, NULL, NULL);
    if (parse_status != ARES_SUCCESS) {
        errorno = PyInt_FromLong((long)parse_status);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    dns_result = PyList_New(0);
    if (!dns_result) {
        PyErr_NoMemory();
        PyErr_WriteUnraisable(Py_None);
        errorno = PyInt_FromLong((long)ARES_ENOMEM);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    for (ptr = hostent->h_addr_list; *ptr != NULL; ptr++) {
        uv_inet_ntop(hostent->h_addrtype, *ptr, ip, sizeof(ip));
        tmp = PYUVString_FromString(ip);
        if (tmp == NULL) {
            break;
        }
        PyList_Append(dns_result, tmp);
        Py_DECREF(tmp);
    }
    ares_free_hostent(hostent);
    errorno = Py_None;
    Py_INCREF(Py_None);

callback:
    result = PyObject_CallFunctionObjArgs(callback, dns_result, errorno, NULL);
    if (result == NULL) {
        PyErr_WriteUnraisable(callback);
    }
    Py_XDECREF(result);
    Py_DECREF(dns_result);
    Py_DECREF(errorno);

    Py_DECREF(callback);
    PyGILState_Release(gstate);
}


static void
query_cname_cb(void *arg, int status,int timeouts, unsigned char *answer_buf, int answer_len)
{
    PyGILState_STATE gstate = PyGILState_Ensure();
    int parse_status;
    struct hostent *hostent;
    PyObject *dns_result, *errorno, *tmp, *result, *callback;

    callback = (PyObject *)arg;
    ASSERT(callback);

    if (status != ARES_SUCCESS) {
        errorno = PyInt_FromLong((long)status);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    parse_status = ares_parse_a_reply(answer_buf, answer_len, &hostent, NULL, NULL);
    if (parse_status != ARES_SUCCESS) {
        errorno = PyInt_FromLong((long)parse_status);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    dns_result = PyList_New(0);
    if (!dns_result) {
        PyErr_NoMemory();
        PyErr_WriteUnraisable(Py_None);
        errorno = PyInt_FromLong((long)ARES_ENOMEM);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    tmp = PYUVString_FromString(hostent->h_name);
    PyList_Append(dns_result, tmp);
    Py_DECREF(tmp);
    ares_free_hostent(hostent);
    errorno = Py_None;
    Py_INCREF(Py_None);

callback:
    result = PyObject_CallFunctionObjArgs(callback, dns_result, errorno, NULL);
    if (result == NULL) {
        PyErr_WriteUnraisable(callback);
    }
    Py_XDECREF(result);
    Py_DECREF(dns_result);
    Py_DECREF(errorno);

    Py_DECREF(callback);
    PyGILState_Release(gstate);
}


static void
query_mx_cb(void *arg, int status,int timeouts, unsigned char *answer_buf, int answer_len)
{
    PyGILState_STATE gstate = PyGILState_Ensure();
    int parse_status;
    struct ares_mx_reply *mx_reply, *mx_ptr;
    PyObject *dns_result, *errorno, *tmp, *result, *callback;

    callback = (PyObject *)arg;
    ASSERT(callback);

    if (status != ARES_SUCCESS) {
        errorno = PyInt_FromLong((long)status);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    parse_status = ares_parse_mx_reply(answer_buf, answer_len, &mx_reply);
    if (parse_status != ARES_SUCCESS) {
        errorno = PyInt_FromLong((long)parse_status);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    dns_result = PyList_New(0);
    if (!dns_result) {
        PyErr_NoMemory();
        PyErr_WriteUnraisable(Py_None);
        errorno = PyInt_FromLong((long)ARES_ENOMEM);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    for (mx_ptr = mx_reply; mx_ptr != NULL; mx_ptr = mx_ptr->next) {
        tmp = PyStructSequence_New(&AresQueryMXResultType);
        if (tmp == NULL) {
            break;
        }
        PyStructSequence_SET_ITEM(tmp, 0, PYUVString_FromString(mx_ptr->host));
        PyStructSequence_SET_ITEM(tmp, 1, PyInt_FromLong((long)mx_ptr->priority));
        PyList_Append(dns_result, tmp);
        Py_DECREF(tmp);
    }
    ares_free_data(mx_reply);
    errorno = Py_None;
    Py_INCREF(Py_None);

callback:
    result = PyObject_CallFunctionObjArgs(callback, dns_result, errorno, NULL);
    if (result == NULL) {
        PyErr_WriteUnraisable(callback);
    }
    Py_XDECREF(result);
    Py_DECREF(dns_result);
    Py_DECREF(errorno);

    Py_DECREF(callback);
    PyGILState_Release(gstate);
}


static void
query_ns_cb(void *arg, int status,int timeouts, unsigned char *answer_buf, int answer_len)
{
    PyGILState_STATE gstate = PyGILState_Ensure();
    int parse_status;
    char **ptr;
    struct hostent *hostent;
    PyObject *dns_result, *errorno, *tmp, *result, *callback;

    callback = (PyObject *)arg;
    ASSERT(callback);

    if (status != ARES_SUCCESS) {
        errorno = PyInt_FromLong((long)status);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    parse_status = ares_parse_ns_reply(answer_buf, answer_len, &hostent);
    if (parse_status != ARES_SUCCESS) {
        errorno = PyInt_FromLong((long)parse_status);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    dns_result = PyList_New(0);
    if (!dns_result) {
        PyErr_NoMemory();
        PyErr_WriteUnraisable(Py_None);
        errorno = PyInt_FromLong((long)ARES_ENOMEM);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    for (ptr = hostent->h_aliases; *ptr != NULL; ptr++) {
        tmp = PYUVString_FromString(*ptr);
        if (tmp == NULL) {
            break;
        }
        PyList_Append(dns_result, tmp);
        Py_DECREF(tmp);
    }
    ares_free_hostent(hostent);
    errorno = Py_None;
    Py_INCREF(Py_None);

callback:
    result = PyObject_CallFunctionObjArgs(callback, dns_result, errorno, NULL);
    if (result == NULL) {
        PyErr_WriteUnraisable(callback);
    }
    Py_XDECREF(result);
    Py_DECREF(dns_result);
    Py_DECREF(errorno);

    Py_DECREF(callback);
    PyGILState_Release(gstate);
}


static void
query_txt_cb(void *arg, int status,int timeouts, unsigned char *answer_buf, int answer_len)
{
    PyGILState_STATE gstate = PyGILState_Ensure();
    int parse_status;
    struct ares_txt_reply *txt_reply, *txt_ptr;
    PyObject *dns_result, *errorno, *tmp, *result, *callback;

    callback = (PyObject *)arg;
    ASSERT(callback);

    if (status != ARES_SUCCESS) {
        errorno = PyInt_FromLong((long)status);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    parse_status = ares_parse_txt_reply(answer_buf, answer_len, &txt_reply);
    if (parse_status != ARES_SUCCESS) {
        errorno = PyInt_FromLong((long)parse_status);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    dns_result = PyList_New(0);
    if (!dns_result) {
        PyErr_NoMemory();
        PyErr_WriteUnraisable(Py_None);
        errorno = PyInt_FromLong((long)ARES_ENOMEM);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    for (txt_ptr = txt_reply; txt_ptr != NULL; txt_ptr = txt_ptr->next) {
        tmp = PYUVString_FromString((const char *)txt_ptr->txt);
        if (tmp == NULL) {
            break;
        }
        PyList_Append(dns_result, tmp);
        Py_DECREF(tmp);
    }
    ares_free_data(txt_reply);
    errorno = Py_None;
    Py_INCREF(Py_None);

callback:
    result = PyObject_CallFunctionObjArgs(callback, dns_result, errorno, NULL);
    if (result == NULL) {
        PyErr_WriteUnraisable(callback);
    }
    Py_XDECREF(result);
    Py_DECREF(dns_result);
    Py_DECREF(errorno);

    Py_DECREF(callback);
    PyGILState_Release(gstate);
}


static void
query_srv_cb(void *arg, int status,int timeouts, unsigned char *answer_buf, int answer_len)
{
    PyGILState_STATE gstate = PyGILState_Ensure();
    int parse_status;
    struct ares_srv_reply *srv_reply, *srv_ptr;
    PyObject *dns_result, *errorno, *tmp, *result, *callback;

    callback = (PyObject *)arg;
    ASSERT(callback);

    if (status != ARES_SUCCESS) {
        errorno = PyInt_FromLong((long)status);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    parse_status = ares_parse_srv_reply(answer_buf, answer_len, &srv_reply);
    if (parse_status != ARES_SUCCESS) {
        errorno = PyInt_FromLong((long)parse_status);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    dns_result = PyList_New(0);
    if (!dns_result) {
        PyErr_NoMemory();
        PyErr_WriteUnraisable(Py_None);
        errorno = PyInt_FromLong((long)ARES_ENOMEM);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    for (srv_ptr = srv_reply; srv_ptr != NULL; srv_ptr = srv_ptr->next) {
        tmp = PyStructSequence_New(&AresQuerySRVResultType);
        if (tmp == NULL) {
            break;
        }
        PyStructSequence_SET_ITEM(tmp, 0, PYUVString_FromString(srv_ptr->host));
        PyStructSequence_SET_ITEM(tmp, 1, PyInt_FromLong((long)srv_ptr->port));
        PyStructSequence_SET_ITEM(tmp, 2, PyInt_FromLong((long)srv_ptr->priority));
        PyStructSequence_SET_ITEM(tmp, 3, PyInt_FromLong((long)srv_ptr->weight));
        PyList_Append(dns_result, tmp);
        Py_DECREF(tmp);
    }
    ares_free_data(srv_reply);
    errorno = Py_None;
    Py_INCREF(Py_None);

callback:
    result = PyObject_CallFunctionObjArgs(callback, dns_result, errorno, NULL);
    if (result == NULL) {
        PyErr_WriteUnraisable(callback);
    }
    Py_XDECREF(result);
    Py_DECREF(dns_result);
    Py_DECREF(errorno);

    Py_DECREF(callback);
    PyGILState_Release(gstate);
}


static void
query_naptr_cb(void *arg, int status,int timeouts, unsigned char *answer_buf, int answer_len)
{
    PyGILState_STATE gstate = PyGILState_Ensure();
    int parse_status;
    struct ares_naptr_reply *naptr_reply, *naptr_ptr;
    PyObject *dns_result, *errorno, *tmp, *result, *callback;

    callback = (PyObject *)arg;
    ASSERT(callback);

    if (status != ARES_SUCCESS) {
        errorno = PyInt_FromLong((long)status);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    parse_status = ares_parse_naptr_reply(answer_buf, answer_len, &naptr_reply);
    if (parse_status != ARES_SUCCESS) {
        errorno = PyInt_FromLong((long)parse_status);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    dns_result = PyList_New(0);
    if (!dns_result) {
        PyErr_NoMemory();
        PyErr_WriteUnraisable(Py_None);
        errorno = PyInt_FromLong((long)ARES_ENOMEM);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    for (naptr_ptr = naptr_reply; naptr_ptr != NULL; naptr_ptr = naptr_ptr->next) {
        tmp = PyStructSequence_New(&AresQueryNAPTRResultType);
        if (tmp == NULL) {
            break;
        }
        PyStructSequence_SET_ITEM(tmp, 0, PyInt_FromLong((long)naptr_ptr->order));
        PyStructSequence_SET_ITEM(tmp, 1, PyInt_FromLong((long)naptr_ptr->preference));
        PyStructSequence_SET_ITEM(tmp, 2, PYUVString_FromString((char *)naptr_ptr->flags));
        PyStructSequence_SET_ITEM(tmp, 3, PYUVString_FromString((char *)naptr_ptr->service));
        PyStructSequence_SET_ITEM(tmp, 4, PYUVString_FromString((char *)naptr_ptr->regexp));
        PyStructSequence_SET_ITEM(tmp, 5, PYUVString_FromString(naptr_ptr->replacement));
        PyList_Append(dns_result, tmp);
        Py_DECREF(tmp);
    }
    ares_free_data(naptr_reply);
    errorno = Py_None;
    Py_INCREF(Py_None);

callback:
    result = PyObject_CallFunctionObjArgs(callback, dns_result, errorno, NULL);
    if (result == NULL) {
        PyErr_WriteUnraisable(callback);
    }
    Py_XDECREF(result);
    Py_DECREF(dns_result);
    Py_DECREF(errorno);

    Py_DECREF(callback);
    PyGILState_Release(gstate);
}


static void
host_cb(void *arg, int status, int timeouts, struct hostent *hostent)
{
    PyGILState_STATE gstate = PyGILState_Ensure();
    char ip[INET6_ADDRSTRLEN];
    char **ptr;
    PyObject *callback, *dns_name, *errorno, *dns_aliases, *dns_addrlist, *dns_result, *tmp, *result;

    callback = (PyObject *)arg;
    ASSERT(callback);

    if (status != ARES_SUCCESS) {
        errorno = PyInt_FromLong((long)status);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    dns_aliases = PyList_New(0);
    dns_addrlist = PyList_New(0);
    dns_result = PyStructSequence_New(&AresHostResultType);

    if (!(dns_aliases && dns_addrlist && dns_result)) {
        PyErr_NoMemory();
        PyErr_WriteUnraisable(Py_None);
        Py_XDECREF(dns_aliases);
        Py_XDECREF(dns_addrlist);
        Py_XDECREF(dns_result);
        errorno = PyInt_FromLong((long)ARES_ENOMEM);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    for (ptr = hostent->h_aliases; *ptr != NULL; ptr++) {
        if (*ptr != hostent->h_name && strcmp(*ptr, hostent->h_name)) {
            tmp = PYUVString_FromString(*ptr);
            if (tmp == NULL) {
                break;
            }
            PyList_Append(dns_aliases, tmp);
            Py_DECREF(tmp);
        }
    }
    for (ptr = hostent->h_addr_list; *ptr != NULL; ptr++) {
        if (hostent->h_addrtype == AF_INET) {
            uv_inet_ntop(AF_INET, *ptr, ip, INET_ADDRSTRLEN);
            tmp = PYUVString_FromString(ip);
        } else if (hostent->h_addrtype == AF_INET6) {
            uv_inet_ntop(AF_INET6, *ptr, ip, INET6_ADDRSTRLEN);
            tmp = PYUVString_FromString(ip);
        } else {
            continue;
        }
        if (tmp == NULL) {
            break;
        }
        PyList_Append(dns_addrlist, tmp);
        Py_DECREF(tmp);
    }
    dns_name = PYUVString_FromString(hostent->h_name);

    PyStructSequence_SET_ITEM(dns_result, 0, dns_name);
    PyStructSequence_SET_ITEM(dns_result, 1, dns_aliases);
    PyStructSequence_SET_ITEM(dns_result, 2, dns_addrlist);
    errorno = Py_None;
    Py_INCREF(Py_None);

callback:
    result = PyObject_CallFunctionObjArgs(callback, dns_result, errorno, NULL);
    if (result == NULL) {
        PyErr_WriteUnraisable(callback);
    }
    Py_XDECREF(result);
    Py_DECREF(dns_result);
    Py_DECREF(errorno);

    Py_DECREF(callback);
    PyGILState_Release(gstate);
}


static void
nameinfo_cb(void *arg, int status, int timeouts, char *node, char *service)
{
    PyGILState_STATE gstate = PyGILState_Ensure();
    PyObject *callback, *errorno, *dns_node, *dns_service, *dns_result, *result;

    callback = (PyObject *)arg;
    ASSERT(callback);

    if (status != ARES_SUCCESS) {
        errorno = PyInt_FromLong((long)status);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    dns_result = PyStructSequence_New(&AresNameinfoResultType);
    if (!dns_result) {
        PyErr_NoMemory();
        PyErr_WriteUnraisable(Py_None);
        errorno = PyInt_FromLong((long)ARES_ENOMEM);
        dns_result = Py_None;
        Py_INCREF(Py_None);
        goto callback;
    }

    dns_node = PYUVString_FromString(node);
    if (service) {
        dns_service = PYUVString_FromString(service);
    } else {
        dns_service = Py_None;
        Py_INCREF(Py_None);
    }

    PyStructSequence_SET_ITEM(dns_result, 0, dns_node);
    PyStructSequence_SET_ITEM(dns_result, 1, dns_service);
    errorno = Py_None;
    Py_INCREF(Py_None);

callback:
    result = PyObject_CallFunctionObjArgs(callback, dns_result, errorno, NULL);
    if (result == NULL) {
        PyErr_WriteUnraisable(callback);
    }
    Py_XDECREF(result);
    Py_DECREF(dns_result);
    Py_DECREF(errorno);

    Py_DECREF(callback);
    PyGILState_Release(gstate);
}


static PyObject *
Channel_func_query(Channel *self, PyObject *args)
{
    char *name;
    int query_type;
    PyObject *callback;

    CHECK_CHANNEL(self);

    if (!PyArg_ParseTuple(args, "isO:query_a", &query_type, &name, &callback)) {
        return NULL;
    }

    if (!PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "a callable is required");
        return NULL;
    }

    Py_INCREF(callback);

    switch (query_type) {
        case T_A:
        {
            ares_query(self->channel, name, C_IN, T_A, &query_a_cb, (void *)callback);
            break;
        }

        case T_AAAA:
        {
            ares_query(self->channel, name, C_IN, T_AAAA, &query_aaaa_cb, (void *)callback);
            break;
        }

        case T_CNAME:
        {
            ares_query(self->channel, name, C_IN, T_CNAME, &query_cname_cb, (void *)callback);
            break;
        }

        case T_MX:
        {
            ares_query(self->channel, name, C_IN, T_MX, &query_mx_cb, (void *)callback);
            break;
        }

        case T_NAPTR:
        {
            ares_query(self->channel, name, C_IN, T_NAPTR, &query_naptr_cb, (void *)callback);
            break;
        }

        case T_NS:
        {
            ares_query(self->channel, name, C_IN, T_NS, &query_ns_cb, (void *)callback);
            break;
        }

        case T_SRV:
        {
            ares_query(self->channel, name, C_IN, T_SRV, &query_srv_cb, (void *)callback);
            break;
        }

        case T_TXT:
        {
            ares_query(self->channel, name, C_IN, T_TXT, &query_txt_cb, (void *)callback);
            break;
        }

        default:
        {
            Py_DECREF(callback);
            PyErr_SetString(PyExc_AresError, "invalid query type specified");
            return NULL;
        }
    }

    Py_RETURN_NONE;
}


static PyObject *
Channel_func_gethostbyname(Channel *self, PyObject *args, PyObject *kwargs)
{
    char *name;
    PyObject *callback;

    if (!PyArg_ParseTuple(args, "sO:gethostbyname", &name, &callback)) {
        return NULL;
    }

    if (!PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "a callable is required");
        return NULL;
    }

    Py_INCREF(callback);
    ares_gethostbyname(self->channel, name, AF_INET, &host_cb, (void *)callback);

    Py_RETURN_NONE;
}


static PyObject *
Channel_func_gethostbyaddr(Channel *self, PyObject *args, PyObject *kwargs)
{
    char *name;
    int family, length;
    void *address;
    struct in_addr addr4;
    struct in6_addr addr6;
    PyObject *callback;

    if (!PyArg_ParseTuple(args, "sO:gethostbyaddr", &name, &callback)) {
        return NULL;
    }

    if (!PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "a callable is required");
        return NULL;
    }

    if (uv_inet_pton(AF_INET, name, &addr4) == 1) {
        family = AF_INET;
        length = sizeof(struct in_addr);
        address = (void *)&addr4;
    } else if (uv_inet_pton(AF_INET6, name, &addr6) == 1) {
        family = AF_INET6;
        length = sizeof(struct in6_addr);
        address = (void *)&addr6;
    } else {
        PyErr_SetString(PyExc_ValueError, "invalid IP address");
        return NULL;
    }

    Py_INCREF(callback);
    ares_gethostbyaddr(self->channel, address, length, family, &host_cb, (void *)callback);

    Py_RETURN_NONE;
}


static PyObject *
Channel_func_getnameinfo(Channel *self, PyObject *args, PyObject *kwargs)
{
    char *addr;
    int port, flags, length;
    struct in_addr addr4;
    struct in6_addr addr6;
    struct sockaddr *sa;
    struct sockaddr_in sa4;
    struct sockaddr_in6 sa6;
    PyObject *callback;

    if (!PyArg_ParseTuple(args, "(si)iO:getnameinfo", &addr, &port, &flags, &callback)) {
        return NULL;
    }

    if (!PyCallable_Check(callback)) {
        PyErr_SetString(PyExc_TypeError, "a callable is required");
        return NULL;
    }

    if (port < 0 || port > 65536) {
        PyErr_SetString(PyExc_ValueError, "port must be between 0 and 65536");
        return NULL;
    }

    if (uv_inet_pton(AF_INET, addr, &addr4) == 1) {
        sa4 = uv_ip4_addr(addr, port);
        sa = (struct sockaddr *)&sa4;
        length = sizeof(struct sockaddr_in);
    } else if (uv_inet_pton(AF_INET6, addr, &addr6) == 1) {
        sa6 = uv_ip6_addr(addr, port);
        sa = (struct sockaddr *)&sa6;
        length = sizeof(struct sockaddr_in6);
    } else {
        PyErr_SetString(PyExc_ValueError, "invalid IP address");
        return NULL;
    }

    Py_INCREF(callback);
    ares_getnameinfo(self->channel, sa, length, flags, &nameinfo_cb, (void *)callback);

    Py_RETURN_NONE;
}


static PyObject *
Channel_func_cancel(Channel *self)
{
    CHECK_CHANNEL(self);
    ares_cancel(self->channel);
    Py_RETURN_NONE;
}


static PyObject *
Channel_func_destroy(Channel *self)
{
    CHECK_CHANNEL(self);
    ares_destroy(self->channel);
    self->channel = NULL;
    Py_RETURN_NONE;
}


static PyObject *
Channel_func_set_local_ip4(Channel *self, PyObject *args)
{
    CHECK_CHANNEL(self);
    // TODO
    Py_RETURN_NONE;
}


static PyObject *
Channel_func_set_local_ip6(Channel *self, PyObject *args)
{
    CHECK_CHANNEL(self);
    // TODO
    Py_RETURN_NONE;
}


static PyObject *
Channel_func_set_local_dev(Channel *self, PyObject *args)
{
    CHECK_CHANNEL(self);
    // TODO
    Py_RETURN_NONE;
}


static PyObject *
Channel_func_process_fd(Channel *self, PyObject *args)
{
    long read_fd, write_fd;

    CHECK_CHANNEL(self);

    if (!PyArg_ParseTuple(args, "ll:process_fd", &read_fd, &write_fd)) {
        return NULL;
    }

    ares_process_fd(self->channel, (ares_socket_t)read_fd, (ares_socket_t)write_fd);
    Py_RETURN_NONE;
}


static PyObject *
fdset2list(fd_set set)
{
    int i;
    PyObject *lst;
    
    lst = PyList_New(0);
    if (!lst) {
        PyErr_NoMemory();
        return NULL;
    }

    for (i = 0; i < FD_SETSIZE; i++) {
        if (FD_ISSET(i, &set)) {
            PyList_Append(lst, PyInt_FromLong((long)i));
            // TODO: check for errors
        }
    }
    return lst;
}

static PyObject *
Channel_func_fds(Channel *self)
{
    int nfds;
    fd_set read_fds, write_fds;
    PyObject *tpl, *rfds, *wfds;

    CHECK_CHANNEL(self);

    tpl = PyTuple_New(2);
    if (!tpl) {
        PyErr_NoMemory();
        return NULL;
    }

    FD_ZERO(&read_fds);
    FD_ZERO(&write_fds);

    nfds = ares_fds(self->channel, &read_fds, &write_fds);
    if (nfds == 0) {
        rfds = PyList_New(0);
        wfds = PyList_New(0);
    } else {
        rfds = fdset2list(read_fds);
        wfds = fdset2list(write_fds);
    }

    if (!rfds || !wfds) {
        Py_DECREF(tpl);
        Py_XDECREF(rfds);
        Py_XDECREF(wfds);
        return NULL;
    }

    PyTuple_SET_ITEM(tpl, 0, rfds);
    PyTuple_SET_ITEM(tpl, 1, wfds);
    return tpl;
}


/* borrowed from signalmodule.c */
static inline void
timeval_from_double(double d, struct timeval *tv)
{
    tv->tv_sec = floor(d);
    tv->tv_usec = fmod(d, 1.0) * 1000000.0;
}

static inline double
double_from_timeval(struct timeval *tv)
{
    return tv->tv_sec + (double)(tv->tv_usec / 1000000.0);
}

static PyObject *
Channel_func_timeout(Channel *self, PyObject *args)
{
    double timeout;
    struct timeval tv, *tvp;

    CHECK_CHANNEL(self);

    if (!PyArg_ParseTuple(args, "|d:timeout", &timeout)) {
        return NULL;
    }

    // TODO
    tvp = ares_timeout(self->channel, NULL, &tv);
    return PyFloat_FromDouble(double_from_timeval(tvp));
}


static int
set_nameservers(Channel *self, PyObject *value)
{
    char *server;
    int i, r, length, ret;
    struct ares_addr_node *servers;
    PyObject *server_list = value;
    PyObject *item;

    ret = 0;

    if (!PyList_Check(server_list)) {
        PyErr_SetString(PyExc_TypeError, "servers argument must be a list");
	return -1;
    }

    length = PyList_Size(server_list);
    servers = PyMem_Malloc(sizeof(struct ares_addr_node) * length);

    for (i = 0; i < length; i++) {
        item = PyList_GetItem(server_list, i);
        if (!item) {
            ret = -1;
            goto end;
        }

        server = PyString_AsString(item);
        if (!server) {
            ret = -1;
            goto end;
        }

        if (uv_inet_pton(AF_INET, server, &servers[i].addr.addr4) == 1) {
            servers[i].family = AF_INET;
        } else if (uv_inet_pton(AF_INET6, server, &servers[i].addr.addr6) == 1) {
            servers[i].family = AF_INET6;
        } else {
            PyErr_SetString(PyExc_ValueError, "invalid IP address");
            ret = -1;
            goto end;
        }

        if (i > 0) {
            servers[i-1].next = &servers[i];
        }
    }

    if (length > 0) {
        servers[length-1].next = NULL;
    } else {
        servers = NULL;
    }

    r = ares_set_servers(self->channel, servers);
    if (r != 0) {
        PyErr_SetString(PyExc_AresError, "error setting nameservers");
        ret = -1;
    }

end:
    PyMem_Free(servers);
    return ret;
}


static PyObject *
Channel_servers_get(Channel *self, void *closure)
{
    int r;
    char ip[INET6_ADDRSTRLEN];
    struct ares_addr_node *server, *servers;
    PyObject *server_list;
    PyObject *tmp;

    UNUSED_ARG(closure);

    if (!self->channel) {
        PyErr_SetString(PyExc_AresError, "Channel has already been destroyed");
        return NULL;
    }

    server_list = PyList_New(0);
    if (!server_list) {
        PyErr_NoMemory();
        return NULL;
    }

    r = ares_get_servers(self->channel, &servers);
    if (r != 0) {
        PyErr_SetString(PyExc_AresError, "error getting c-ares nameservers");
        return NULL;
    }

    for (server = servers; server != NULL; server = server->next) {
        if (server->family == AF_INET) {
            uv_inet_ntop(AF_INET, &(server->addr.addr4), ip, INET_ADDRSTRLEN);
            tmp = PYUVString_FromString(ip);
        } else {
            uv_inet_ntop(AF_INET6, &(server->addr.addr6), ip, INET6_ADDRSTRLEN);
            tmp = PYUVString_FromString(ip);
        }
        if (tmp == NULL) {
            break;
        }
        r = PyList_Append(server_list, tmp);
        Py_DECREF(tmp);
        if (r != 0) {
            break;
        }
    }

    return server_list;
}


static int
Channel_servers_set(Channel *self, PyObject *value, void *closure)
{
    UNUSED_ARG(closure);
    if (!self->channel) {
        PyErr_SetString(PyExc_AresError, "Channel has already been destroyed");
        return -1;
    }
    return set_nameservers(self, value);
}



static int
Channel_tp_init(Channel *self, PyObject *args, PyObject *kwargs)
{
    int r, flags, tries, ndots, tcp_port, udp_port, optmask;
    double timeout;
    struct ares_options options;
    PyObject *servers, *domains, *lookups, *sock_state_cb;

    static char *kwlist[] = {"flags", "timeout", "tries", "ndots", "tcp_port", "udp_port", "servers", "domains", "lookups", "sock_state_cb", NULL};

    optmask = 0;
    flags = tries = ndots = tcp_port = udp_port = timeout = -1;
    servers = domains = lookups = sock_state_cb = NULL;

    if (self->channel) {
        PyErr_SetString(PyExc_AresError, "Object already initialized");
        return -1;
    }

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|idiiiiOOOO:__init__", kwlist, &flags, &timeout, &tries, &ndots, &tcp_port, &udp_port, &servers, &domains, &lookups, &sock_state_cb)) {
        return -1;
    }

    if (sock_state_cb && !PyCallable_Check(sock_state_cb)) {
        PyErr_SetString(PyExc_TypeError, "sock_state_cb is not callable");
        return -1;
    }

    r = ares_library_init(ARES_LIB_INIT_ALL);
    if (r != 0) {
        PyErr_SetString(PyExc_AresError, "error initializing c-ares library");
        return -1;
    }
    ares_lib_initialized = True;

    memset(&options, 0, sizeof(struct ares_options));

    if (flags != -1) {
        options.flags = flags;
        optmask |= ARES_OPT_FLAGS;
    }
    if (timeout != -1) {
        options.timeout = (int)timeout * 1000;
        optmask |= ARES_OPT_TIMEOUTMS;
    }
    if (tries != -1) {
        options.tries = tries;
        optmask |= ARES_OPT_TRIES;
    }
    if (ndots != -1) {
        options.ndots = ndots;
        optmask |= ARES_OPT_NDOTS;
    }
    if (tcp_port != -1) {
        options.tcp_port = tcp_port;
        optmask |= ARES_OPT_TCP_PORT;
    }
    if (udp_port != -1) {
        options.udp_port = udp_port;
        optmask |= ARES_OPT_UDP_PORT;
    }
    if (sock_state_cb) {
        options.sock_state_cb = ares__sock_state_cb;
        options.sock_state_cb_data = (void *)self;
        optmask |= ARES_OPT_SOCK_STATE_CB;
        Py_INCREF(sock_state_cb);
        self->sock_state_cb = sock_state_cb;
    }
    // TODO: domains, lookups, sock_sndbug, sock_rcvbuf, sortlist

    r = ares_init_options(&self->channel, &options, optmask);
    if (r != 0) {
        PyErr_SetString(PyExc_AresError, "error setting c-ares channel options");
        goto error;
    }

    if (servers) {
        return set_nameservers(self, servers);
    }

    return 0;

error:
    Py_XDECREF(sock_state_cb);
    return -1;
}



static PyObject *
Channel_tp_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    Channel *self = (Channel *)PyType_GenericNew(type, args, kwargs);
    if (!self) {
        return NULL;
    }
    self->channel = NULL;
    return (PyObject *)self;
}


static int
Channel_tp_traverse(Channel *self, visitproc visit, void *arg)
{
    Py_VISIT(self->sock_state_cb);
    return 0;
}


static int
Channel_tp_clear(Channel *self)
{
    Py_CLEAR(self->sock_state_cb);
    return 0;
}


static void
Channel_tp_dealloc(Channel *self)
{
    if (self->channel) {
        ares_destroy(self->channel);
        self->channel = NULL;
    }
    Channel_tp_clear(self);
    Py_TYPE(self)->tp_free((PyObject *)self);
}


static PyMethodDef
Channel_tp_methods[] = {
    { "gethostbyname", (PyCFunction)Channel_func_gethostbyname, METH_VARARGS|METH_KEYWORDS, "Gethostbyname" },
    { "gethostbyaddr", (PyCFunction)Channel_func_gethostbyaddr, METH_VARARGS|METH_KEYWORDS, "Gethostbyaddr" },
    { "getnameinfo", (PyCFunction)Channel_func_getnameinfo, METH_VARARGS|METH_KEYWORDS, "Getnameinfo" },
    { "query", (PyCFunction)Channel_func_query, METH_VARARGS, "Run a DNS query of the specified type" },
    { "cancel", (PyCFunction)Channel_func_cancel, METH_NOARGS, "Cancel all pending queries on this resolver" },
    { "destroy", (PyCFunction)Channel_func_destroy, METH_NOARGS, "Destroy this channel, it will no longer be usable" },
    { "process_fd", (PyCFunction)Channel_func_process_fd, METH_VARARGS, "Process file descriptors actions" },
    { "set_local_ip4", (PyCFunction)Channel_func_set_local_ip4, METH_VARARGS, "Set source IPv4 address" },
    { "set_local_ip6", (PyCFunction)Channel_func_set_local_ip6, METH_VARARGS, "Set source IPv6 address" },
    { "set_local_dev", (PyCFunction)Channel_func_set_local_dev, METH_VARARGS, "Set source device name" },
    { "fds", (PyCFunction)Channel_func_fds, METH_VARARGS, "Returns the set of file descriptors which the calling application should select on" },
    { "timeout", (PyCFunction)Channel_func_timeout, METH_VARARGS, "Determine polling timeout" },
    { NULL }
};


static PyGetSetDef Channel_tp_getsets[] = {
    {"servers", (getter)Channel_servers_get, (setter)Channel_servers_set, "DNS nameserver", NULL},
    {NULL}
};


static PyTypeObject ChannelType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "pyuv.ares.Channel",                                            /*tp_name*/
    sizeof(Channel),                                                /*tp_basicsize*/
    0,                                                              /*tp_itemsize*/
    (destructor)Channel_tp_dealloc,                                 /*tp_dealloc*/
    0,                                                              /*tp_print*/
    0,                                                              /*tp_getattr*/
    0,                                                              /*tp_setattr*/
    0,                                                              /*tp_compare*/
    0,                                                              /*tp_repr*/
    0,                                                              /*tp_as_number*/
    0,                                                              /*tp_as_sequence*/
    0,                                                              /*tp_as_mapping*/
    0,                                                              /*tp_hash */
    0,                                                              /*tp_call*/
    0,                                                              /*tp_str*/
    0,                                                              /*tp_getattro*/
    0,                                                              /*tp_setattro*/
    0,                                                              /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,                        /*tp_flags*/
    0,                                                              /*tp_doc*/
    (traverseproc)Channel_tp_traverse,                              /*tp_traverse*/
    (inquiry)Channel_tp_clear,                                      /*tp_clear*/
    0,                                                              /*tp_richcompare*/
    0,                                                              /*tp_weaklistoffset*/
    0,                                                              /*tp_iter*/
    0,                                                              /*tp_iternext*/
    Channel_tp_methods,                                             /*tp_methods*/
    0,                                                              /*tp_members*/
    Channel_tp_getsets,                                             /*tp_getsets*/
    0,                                                              /*tp_base*/
    0,                                                              /*tp_dict*/
    0,                                                              /*tp_descr_get*/
    0,                                                              /*tp_descr_set*/
    0,                                                              /*tp_dictoffset*/
    (initproc)Channel_tp_init,                                      /*tp_init*/
    0,                                                              /*tp_alloc*/
    Channel_tp_new,                                                 /*tp_new*/
};

#ifdef PYUV_PYTHON3
static PyModuleDef pyuv_ares_module = {
    PyModuleDef_HEAD_INIT,
    "pyuv.ares",            /*m_name*/
    NULL,                   /*m_doc*/
    -1,                     /*m_size*/
    NULL,                   /*m_methods*/
};
#endif

PyObject *
init_ares(void)
{
    PyObject *module;
#ifdef PYUV_PYTHON3
    module = PyModule_Create(&pyuv_ares_module);
#else
    module = Py_InitModule("pyuv.cares", NULL);
#endif

    if (module == NULL) {
        return NULL;
    }

    /* Cleanup ares on exit */
    Py_AtExit(_ares_cleanup);

    /* PyStructSequence types */
    if (AresHostResultType.tp_name == 0)
        PyStructSequence_InitType(&AresHostResultType, &ares_host_result_desc);
    if (AresNameinfoResultType.tp_name == 0)
        PyStructSequence_InitType(&AresNameinfoResultType, &ares_nameinfo_result_desc);
    if (AresQueryMXResultType.tp_name == 0)
        PyStructSequence_InitType(&AresQueryMXResultType, &ares_query_mx_result_desc);
    if (AresQuerySRVResultType.tp_name == 0)
        PyStructSequence_InitType(&AresQuerySRVResultType, &ares_query_srv_result_desc);
    if (AresQueryNAPTRResultType.tp_name == 0)
        PyStructSequence_InitType(&AresQueryNAPTRResultType, &ares_query_naptr_result_desc);

    PyModule_AddIntMacro(module, ARES_NI_NOFQDN);
    PyModule_AddIntMacro(module, ARES_NI_NUMERICHOST);
    PyModule_AddIntMacro(module, ARES_NI_NAMEREQD);
    PyModule_AddIntMacro(module, ARES_NI_NUMERICSERV);
    PyModule_AddIntMacro(module, ARES_NI_DGRAM);
    PyModule_AddIntMacro(module, ARES_NI_TCP);
    PyModule_AddIntMacro(module, ARES_NI_UDP);
    PyModule_AddIntMacro(module, ARES_NI_SCTP);
    PyModule_AddIntMacro(module, ARES_NI_DCCP);
    PyModule_AddIntMacro(module, ARES_NI_NUMERICSCOPE);
    PyModule_AddIntMacro(module, ARES_NI_LOOKUPHOST);
    PyModule_AddIntMacro(module, ARES_NI_LOOKUPSERVICE);
    PyModule_AddIntMacro(module, ARES_NI_IDN);
    PyModule_AddIntMacro(module, ARES_NI_IDN_ALLOW_UNASSIGNED);
    PyModule_AddIntMacro(module, ARES_NI_IDN_USE_STD3_ASCII_RULES);

    PyModule_AddIntMacro(module, ARES_SOCKET_BAD);

    PyModule_AddIntConstant(module, "QUERY_TYPE_A", T_A);
    PyModule_AddIntConstant(module, "QUERY_TYPE_AAAA", T_AAAA);
    PyModule_AddIntConstant(module, "QUERY_TYPE_CNAME", T_CNAME);
    PyModule_AddIntConstant(module, "QUERY_TYPE_MX", T_MX);
    PyModule_AddIntConstant(module, "QUERY_TYPE_NAPTR", T_NAPTR);
    PyModule_AddIntConstant(module, "QUERY_TYPE_NS", T_NS);
    PyModule_AddIntConstant(module, "QUERY_TYPE_SRV", T_SRV);
    PyModule_AddIntConstant(module, "QUERY_TYPE_TXT", T_TXT);

    PyUVModule_AddType(module, "Channel", &ChannelType);

    return module;
}
