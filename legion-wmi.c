#include <linux/kernel.h>
#include <linux/acpi.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/wmi.h>
// procfs stuff
#include "procfs.c"

// #define pr_fmt(fmt) KBUILD_MODNAME “: ” fmt

struct wmi_device *legion_device;

#define LEGION_WMI_GAME_ZONE_GUID				"887B54E3-DDDC-4B2C-8B88-68A26A8835D0"
#define LEGION_WMI_FAN_MODE_EVENT_GUID			"D320289E-8FEA-41E0-86F9-611D83151B5F"

static const struct wmi_device_id legion_wmi_id_table[] = {
	{ LEGION_WMI_GAME_ZONE_GUID, NULL },
	// { LEGION_WMI_FAN_MODE_EVENT_GUID, NULL },
	{ }
};

enum legion_wmi_gamezone_method {
	LEGION_WMI_GAMEZONE_METHOD					= 0xAA,	// WMAA, DSDT
};

enum legion_wmi_gamezone_command {
	LEGION_WMI_GAMEZONE_GET_PRODUCT_ID			= 0x30,
	LEGION_WMI_GAMEZONE_GET_REAL_THERMAL_MODE	= 0x37,
	LEGION_WMI_GAMEZONE_SET_KB_LIGHT			= 0x24,
	LEGION_WMI_GAMEZONE_SET_TCHPAD				= 0x19,

	// smart fan mode
	LEGION_WMI_GAMEZONE_SET_SMARTFANMODE 		= 0x2C,
};

struct legion_wmi_args_3i {
	u32 arg1;
	u32 arg2;
	u32 arg3;
};

struct legion_wmi_args_2i {
	u32 arg1;
	u32 arg2;
};

struct legion_wmi_args_1i {
	u32 arg1;
};

// ACPI stuff
// weird 177 bit argument it seems -- refer to SSDT16
struct legion_nvda0820_npcf_arg3 {
	u64 part1;
	u64 part2;
	u32 part3;
	u16 part4;
	bool part5;
};

static void legion_wmi_notify(u32 value, void *context)
{
	// wmi notification handler left unimplemented
	// would likely make an ACPI call here

	pr_warn("LEGION WMI: something has happened. Value %d", value);
}

static int legion_wmi_perform_query(struct wmi_device *wdev,
				      enum legion_wmi_gamezone_method method_id,
				      const struct acpi_buffer *in, struct acpi_buffer *out)
{
	acpi_status ret = wmidev_evaluate_method(wdev, 0x0, method_id, in, out);

	if (ACPI_FAILURE(ret)) {
		dev_warn(&wdev->dev, "WMI query failed with error: %d\n", ret);
		return -EIO;
	}

	return 0;
}

static int legion_wmi_query_integer(struct wmi_device *wdev,
						enum legion_wmi_gamezone_method method_id,
						const struct acpi_buffer *in, u32 *res)
{
	union acpi_object *obj;
	struct acpi_buffer result = { ACPI_ALLOCATE_BUFFER, NULL };
	int ret;

	ret = legion_wmi_perform_query(wdev, method_id, in, &result);
	if (ret)
		return ret;
	obj = result.pointer;
	if (obj && obj->type == ACPI_TYPE_INTEGER)
		*res = obj->integer.value;
	else
		ret = -EIO;
	kfree(result.pointer);
	return ret;
}


/** procfs write callback. Called when writing into /proc/acpi/call.
*/
static ssize_t acpi_proc_write( struct file *filp, const char __user *buff,
    size_t len, loff_t *data )
{
    char input[2 * BUFFER_SIZE] = { '\0' };
    union acpi_object *args;
    int nargs, i;
    char *method;

    if (len > sizeof(input) - 1) {
        printk(KERN_ERR "legion_acpi_call: Input too long! (%lu)\n", len);
        return -ENOSPC;
    }

    if (copy_from_user( input, buff, len )) {
        return -EFAULT;
    }
    input[len] = '\0';
    if (input[len-1] == '\n')
        input[len-1] = '\0';

	pr_warn("legion_acpi_call: procfs write is %s", input);

	if (strcmp(input, "hello") == 0)
	{
		pr_warn("legion_acpi_call: hello.");
	}
	else if (strcmp(input, "quiet") == 0
		|| strcmp(input, "balanced") == 0
		|| strcmp(input, "perf") == 0)
		{

		u32 mode = (strcmp(input, "quiet") == 0)? 0x1:((strcmp(input, "balanced") == 0))? 0x2:0x3;

		struct legion_wmi_args_1i args = {
			.arg1 = mode,
		};

		const struct acpi_buffer in = {
			.length = sizeof(args),
			.pointer = &args,
		};

		u32 prod_id;
		acpi_status ret;

		ret = legion_wmi_query_integer(legion_device, LEGION_WMI_GAMEZONE_SET_SMARTFANMODE, &in, &prod_id);
		if (ret == 0) {
			dev_info(&legion_device->dev, "Integer query result is %d", prod_id);
		} else {
			dev_warn(&legion_device->dev, "Integer query failed with err: %d", ret);
		}
	}
	else if (strcmp(input, "nvidia-npcf") == 0)
	{
		acpi_status status;
		acpi_handle handle;
		struct acpi_buffer buffer = { ACPI_ALLOCATE_BUFFER, NULL };
		const char *method = "\\_SB.NPCF.NPCF\0";

		printk(KERN_INFO "legion_acpi_call: Calling %s\n", method);

		// get the handle of the method, must be a fully qualified path
		status = acpi_get_handle(NULL, (acpi_string) method, &handle);

		if (ACPI_FAILURE(status))
		{
			snprintf(result_buffer, BUFFER_SIZE, "Error: %s", acpi_format_exception(status));
			printk(KERN_ERR "legion_acpi_call: Cannot get handle: %s\n", result_buffer);
			return len;
		}

		printk(KERN_INFO "legion_acpi_call: Handle acquisition successful: %d\n", status);

		struct acpi_object_list	input;
		union acpi_object 	in_params[4];

		struct legion_nvda0820_npcf_arg3 arg3 = {
			.part1 = 0x0,
			.part2 = 0x0,
			.part3 = 0x0,
			.part4 = 0x0,
			.part5 = 0x0
		};

		input.count = 4;
		input.pointer = in_params;
		in_params[0].type = ACPI_TYPE_INTEGER;
		in_params[0].integer.value = 0x00;
		in_params[1].type = ACPI_TYPE_INTEGER;
		in_params[1].integer.value = 0x0200;
		in_params[2].type = ACPI_TYPE_INTEGER;
		in_params[2].integer.value = 0x02;
		in_params[3].type = ACPI_TYPE_BUFFER;
		in_params[3].buffer.length = sizeof(arg3);
		in_params[3].buffer.pointer = (u8 *)&arg3;

		// call the method
		status = acpi_evaluate_object(handle, NULL, &input, &buffer);
		if (ACPI_FAILURE(status))
		{
			snprintf(result_buffer, BUFFER_SIZE, "Error: %s", acpi_format_exception(status));
			printk(KERN_ERR "legion_acpi_call: Method call failed: %s\n", result_buffer);
			return len;
		}

		// reset the result buffer
		*result_buffer = '\0';
		acpi_result_to_string(buffer.pointer);
		kfree(buffer.pointer);

		printk(KERN_INFO "legion_acpi_call: Call successful: %s\n", result_buffer);
	}

    return len;
}

static ssize_t acpi_proc_read( struct file *filp, char __user *buff,
            size_t count, loff_t *off )
{
    ssize_t ret;
    int len = strlen(result_buffer);

    // output the current result buffer
    ret = simple_read_from_buffer(buff, count, off, result_buffer, len + 1);

    // initialize the result buffer for later
    strcpy(result_buffer, "not called");

    return ret;
}

static const struct proc_ops proc_acpi_operations = {
        .proc_read     = acpi_proc_read,
        .proc_write    = acpi_proc_write,
};

static int legion_wmi_probe(struct wmi_device *wdev, const void *context)
{
	dev_info(&wdev->dev, "LEGION WMI INIT LMAO");

	if (!wmi_has_guid(LEGION_WMI_FAN_MODE_EVENT_GUID)) {
		dev_warn(&wdev->dev, "No known FAN MODE EVENT WMI GUID found");
		return -ENODEV;
	}

	if (!wmi_has_guid(LEGION_WMI_GAME_ZONE_GUID)) {
		dev_warn(&wdev->dev, "No known GAMEZONE WMI GUID found");
		return -ENODEV;
	}

	int err;

	err = wmi_install_notify_handler(LEGION_WMI_FAN_MODE_EVENT_GUID,
		legion_wmi_notify, NULL);
	if (ACPI_FAILURE(err)) {
		if (err == AE_ALREADY_ACQUIRED)
			dev_warn(&wdev->dev, "AE_ALREADY_ACQUIRED");

		dev_warn(&wdev->dev, "Unable to setup WMI notify handler. err: %d\n", err);
	} else {
		dev_info(&wdev->dev, "Successfully registered wmi notify handler.\n");
	}

	legion_device = wdev;

    struct proc_dir_entry *acpi_entry = proc_create("legion_call",
                                                    0660,
                                                    acpi_root_dir,
                                                    &proc_acpi_operations);
    strcpy(result_buffer, "not called");

    if (acpi_entry == NULL) {
      dev_warn(&wdev->dev, "Couldn't create procfs entry\n");
      return -ENOMEM;
    }

    dev_info(&wdev->dev, "procfs entry at /proc/acpi/legion_call created.\n");

	dev_info(&wdev->dev, "Probe is exiting.\n");

	return 0;
}

static void legion_wmi_remove(struct wmi_device *wdev) {
	int err;

	err = wmi_remove_notify_handler(LEGION_WMI_FAN_MODE_EVENT_GUID);
	if (ACPI_FAILURE(err)) {
		dev_warn(&wdev->dev, "Unable to REMOVE WMI notify handler. err: %d\n", err);
	}

	legion_device = NULL;

    remove_proc_entry("legion_call", acpi_root_dir);

    dev_info(&wdev->dev, "procfs enry removed\n");
}

static struct wmi_driver legion_wmi_driver = {
	.driver = {
		.name = "legion-wmi",
	},
	.id_table = legion_wmi_id_table,
	.probe = legion_wmi_probe,
	.remove = legion_wmi_remove
};

module_wmi_driver(legion_wmi_driver);
MODULE_DEVICE_TABLE(wmi, legion_wmi_id_table);

MODULE_DESCRIPTION("Legion WMI Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.1");
