


void on_key_press(uint8_t key, bool event)
{
	if (USB_is_sleeping())
		USB_wakeup();
	else
		LAYOUT_set_key_state(key, event);
}

void main_task()
{
	bool changed = MATRIX_scan();
	if (changed)
		HID_commit_state();
}

int main(void)
{
	/* initialize with 65 keys */
	LAYOUT_init(65);
	LAYOUT_set((struct layout*)LAYOUT_BEGIN);
	LAYOUT_set_callback(&HID_set_scancode_state);

	MATRIX_init(5, rows, 14, cols, (const uint8_t*)matrix, &on_key_press);

	HID_init();
	HID_commit_state();

}
