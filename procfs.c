/* Copyright (c) 2010: Michal Kottman
   from https://github.com/mkottman/acpi_call
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/acpi.h>

MODULE_LICENSE("GPL");

/* Uncomment the following line to enable debug messages */
/*
#define DEBUG
*/

#define BUFFER_SIZE 256
#define MAX_ACPI_ARGS 16

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
#define HAVE_PROC_CREATE
#endif

extern struct proc_dir_entry *acpi_root_dir;

static char result_buffer[BUFFER_SIZE];

static u8 temporary_buffer[BUFFER_SIZE];

static size_t get_avail_bytes(void) {
    return BUFFER_SIZE - strlen(result_buffer);
}
static char *get_buffer_end(void) {
    return result_buffer + strlen(result_buffer);
}

/** Appends the contents of an acpi_object to the result buffer
@param result   An acpi object holding result data
@returns        0 if the result could fully be saved, a higher value otherwise
*/
static int acpi_result_to_string(union acpi_object *result) {
    if (result->type == ACPI_TYPE_INTEGER) {
        snprintf(get_buffer_end(), get_avail_bytes(),
            "0x%x", (int)result->integer.value);
    } else if (result->type == ACPI_TYPE_STRING) {
        snprintf(get_buffer_end(), get_avail_bytes(),
            "\"%*s\"", result->string.length, result->string.pointer);
    } else if (result->type == ACPI_TYPE_BUFFER) {
        int i;
        // do not store more than data if it does not fit. The first element is
        // just 4 chars, but there is also two bytes from the curly brackets
        int show_values = min((size_t)result->buffer.length, get_avail_bytes() / 6);

        sprintf(get_buffer_end(), "{");
        for (i = 0; i < show_values; i++)
            sprintf(get_buffer_end(),
                i == 0 ? "0x%02x" : ", 0x%02x", result->buffer.pointer[i]);

        if (result->buffer.length > show_values) {
            // if data was truncated, show a trailing comma if there is space
            snprintf(get_buffer_end(), get_avail_bytes(), ",");
            return 1;
        } else {
            // in case show_values == 0, but the buffer is too small to hold
            // more values (i.e. the buffer cannot have anything more than "{")
            snprintf(get_buffer_end(), get_avail_bytes(), "}");
        }
    } else if (result->type == ACPI_TYPE_PACKAGE) {
        int i;
        sprintf(get_buffer_end(), "[");
        for (i=0; i<result->package.count; i++) {
            if (i > 0)
                snprintf(get_buffer_end(), get_avail_bytes(), ", ");

            // abort if there is no more space available
            if (!get_avail_bytes() || acpi_result_to_string(&result->package.elements[i]))
                return 1;
        }
        snprintf(get_buffer_end(), get_avail_bytes(), "]");
    } else {
        snprintf(get_buffer_end(), get_avail_bytes(),
            "Object type 0x%x\n", result->type);
    }

    // return 0 if there are still bytes available, 1 otherwise
    return !get_avail_bytes();
}

/**
@param method   The full name of ACPI method to call
@param argc     The number of parameters
@param argv     A pre-allocated array of arguments of type acpi_object
*/
static void do_acpi_eval(const char * path)
{
    acpi_status status;
    acpi_handle handle;
    struct acpi_object_list arg;
    struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };

    printk(KERN_INFO "acpi_call: Evaluating %s\n", path);

    // call the path
    status = acpi_evaluate_object(NULL, path, NULL, &buffer);
    if (ACPI_FAILURE(status))
    {
        snprintf(result_buffer, BUFFER_SIZE, "Error: %s", acpi_format_exception(status));
        printk(KERN_ERR "acpi_call: Path call failed: %s\n", result_buffer);
        return;
    }

    // reset the result buffer
    *result_buffer = '\0';
    acpi_result_to_string(buffer.pointer);
    kfree(buffer.pointer);

    printk(KERN_INFO "acpi_call: Call successful: %s\n", result_buffer);
}
