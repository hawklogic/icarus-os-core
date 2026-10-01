#include "unity.h"
#include "bsp/board_feedback.h"
#include "icarus/kernel.h"
#include "mock_gpio.h"

static void test_feedback_cell_bounds(void) {
    TEST_ASSERT_TRUE(board_feedback_init());
    TEST_ASSERT_TRUE(board_feedback_cell(0U, 'A'));
    TEST_ASSERT_TRUE(board_feedback_cell(64U, '9'));
    TEST_ASSERT_TRUE(board_feedback_cell(32U, ' '));
    TEST_ASSERT_FALSE(board_feedback_cell(65U, 'A'));
    TEST_ASSERT_FALSE(board_feedback_cell(UINT32_MAX, 'A'));
    TEST_ASSERT_FALSE(board_feedback_cell(0U, '\n'));
    TEST_ASSERT_FALSE(board_feedback_cell(0U, 256U + 'A'));
}
static void test_feedback_pulse_expiry_and_wrap(void) {
    TEST_ASSERT_TRUE(board_feedback_init());
    os_tick_count = UINT32_MAX - 100U;
    TEST_ASSERT_TRUE(board_feedback_pulse(250U));
    TEST_ASSERT_EQUAL(GPIO_PIN_SET, mock_gpio_last_state);
    os_tick_count += 249U;
    TEST_ASSERT_TRUE(board_feedback_pulse(0U));
    TEST_ASSERT_EQUAL(GPIO_PIN_SET, mock_gpio_last_state);
    os_tick_count++;
    TEST_ASSERT_TRUE(board_feedback_pulse(0U));
    TEST_ASSERT_EQUAL(GPIO_PIN_RESET, mock_gpio_last_state);
    TEST_ASSERT_FALSE(board_feedback_pulse(1001U));
    TEST_ASSERT_EQUAL(GPIO_PIN_RESET, mock_gpio_last_state);
}
static void test_feedback_idle_does_not_override_fault_led(void) {
    TEST_ASSERT_TRUE(board_feedback_init());
    mock_gpio_last_state = GPIO_PIN_SET;
    TEST_ASSERT_TRUE(board_feedback_pulse(0U));
    TEST_ASSERT_EQUAL(GPIO_PIN_SET, mock_gpio_last_state);
}
void run_board_feedback_tests(void) {
    RUN_TEST(test_feedback_cell_bounds);
    RUN_TEST(test_feedback_pulse_expiry_and_wrap);
    RUN_TEST(test_feedback_idle_does_not_override_fault_led);
}
