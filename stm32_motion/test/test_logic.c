#include "car_motion.h"
#include "car_protocol.h"
#include "car_uart.h"
#include "car_config.h"
#include "stm32f10x.h"

void USART1_IRQHandler(void);

#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

static void Frame(uint8_t state, uint8_t out[4])
{
    out[0] = 0xAA; out[1] = 0x55; out[2] = state;
    out[3] = Car_Crc8(out, 3);
}

int Test_Run(void)
{
    CarParser parser = {0};
    uint8_t state = 0xFF;
    uint8_t frame[4];
    const uint8_t vectors[][4] = {
        {0xAA, 0x55, 0x00, 0x82},
        {0xAA, 0x55, 0x15, 0xE9},
        {0xAA, 0x55, 0x1F, 0xDF}
    };
    for (unsigned v = 0; v < 3; ++v) {
        CHECK(Car_Crc8(vectors[v], 3) == vectors[v][3]);
    }
    for (unsigned s = 0; s < 32; ++s) {
        Frame((uint8_t)s, frame);
        state = 0xFF;
        for (unsigned i = 0; i < 3; ++i) {
            CHECK(!CarParser_Feed(&parser, frame[i], &state));
            CHECK(state == 0xFF);
        }
        CHECK(CarParser_Feed(&parser, frame[3], &state));
        CHECK(state == s);
    }
    /* 任意一位传输错误均不可生成有效指令，之后可恢复。 */
    Frame(CAR_FORWARD, frame);
    for (unsigned bit = 0; bit < 32; ++bit) {
        CarParser_Reset(&parser);
        state = 0xFF;
        for (unsigned i = 0; i < 4; ++i) {
            uint8_t byte = frame[i];
            if (i == bit / 8) byte ^= (uint8_t)(1U << (bit % 8));
            CHECK(!CarParser_Feed(&parser, byte, &state));
        }
        CHECK(state == 0xFF);
        for (unsigned i = 0; i < 3; ++i) CHECK(!CarParser_Feed(&parser, frame[i], &state));
        CHECK(CarParser_Feed(&parser, frame[3], &state));
        CHECK(state == CAR_FORWARD);
    }
    /* CRC 正确但使用预留位的帧也应拒绝。 */
    for (unsigned s = 32; s < 256; ++s) {
        CarParser_Reset(&parser);
        Frame((uint8_t)s, frame);
        for (unsigned i = 0; i < 4; ++i) CHECK(!CarParser_Feed(&parser, frame[i], &state));
    }
    /* 噪声、半帧、重叠帧头、数据丢失后的恢复。 */
    const uint8_t noise[] = {0x00, 0xAA, 0xAA, 0x55, 0xAA};
    CarParser_Reset(&parser);
    for (unsigned i = 0; i < sizeof(noise); ++i) CHECK(!CarParser_Feed(&parser, noise[i], &state));
    Frame(0, frame);
    for (unsigned i = 0; i < 3; ++i) CHECK(!CarParser_Feed(&parser, frame[i], &state));
    CHECK(CarParser_Feed(&parser, frame[3], &state));
    CHECK(state == 0);
    CHECK(!CarParser_Feed(&parser, 0xAA, &state));
    CHECK(!CarParser_Feed(&parser, 0x55, &state));
    CarParser_Reset(&parser);
    for (unsigned i = 0; i < 3; ++i) CHECK(!CarParser_Feed(&parser, frame[i], &state));
    CHECK(CarParser_Feed(&parser, frame[3], &state));

    /* 每行依次对应运动位 0..15；LED 不改变运动。 */
    const int16_t expected[16][2] = {
        {0,0}, {70,70}, {-70,-70}, {0,0},
        {35,70}, {35,70}, {-70,-35}, {0,0},
        {70,35}, {70,35}, {-35,-70}, {0,0},
        {0,0}, {0,0}, {0,0}, {0,0}
    };
    for (unsigned s = 0; s < 32; ++s) {
        const CarMotion motion = CarMotion_Decode((uint8_t)s, 70);
        CHECK(motion.left == expected[s & 15][0]);
        CHECK(motion.right == expected[s & 15][1]);
    }
    CHECK(CarMotion_Decode(CAR_FORWARD, 255).left == 100);
    CHECK(CarMotion_Decode(CAR_BACKWARD, 255).right == -100);
    CHECK(CarMotion_Decode(CAR_FORWARD, 0).left == 0);
    CHECK(CarMotion_Decode(0x81, 70).left == 0);
    CHECK(CarMotion_Decode(CAR_LEFT, 0).left == 0);
    CHECK(CarMotion_Decode(CAR_LEFT, 0).right == 0);
    CHECK(CarMotion_Decode(CAR_LEFT, 1).left == 1);
    CHECK(CarMotion_Decode(CAR_RIGHT, 1).right == 1);
    CHECK(CarMotion_Decode(CAR_LEFT, 255).left == 50);
    CHECK(CarMotion_Decode(CAR_LEFT, 255).right == 100);
    CHECK(CarMotion_Decode(CAR_RIGHT, 255).left == 100);
    CHECK(CarMotion_Decode(CAR_RIGHT, 255).right == 50);
    for (unsigned speed = 2; speed <= 100; ++speed) {
        const CarMotion left = CarMotion_Decode(CAR_LEFT, (uint8_t)speed);
        const CarMotion right = CarMotion_Decode(CAR_RIGHT, (uint8_t)speed);
        CHECK(left.left > 0 && left.left < left.right);
        CHECK(right.right > 0 && right.right < right.left);
    }

    /* 模拟 USART 寄存器输入，验证真实中断函数与队列读写代码。 */
    uint8_t byte = 0;
    bool lost = false;
    CHECK(!CarUart_Read(&byte, &lost));
    CHECK(!lost);
    for (unsigned round = 0; round < 3; ++round) {
        for (unsigned i = 0; i < CAR_RX_CAPACITY - 1; ++i) {
            USART1->SR = USART_SR_RXNE;
            USART1->DR = i;
            USART1_IRQHandler();
        }
        for (unsigned i = 0; i < CAR_RX_CAPACITY - 1; ++i) {
            CHECK(CarUart_Read(&byte, &lost));
            CHECK(!lost && byte == i);
        }
        CHECK(!CarUart_Read(&byte, &lost));
    }
    for (unsigned i = 0; i < CAR_RX_CAPACITY; ++i) {
        USART1->SR = USART_SR_RXNE;
        USART1->DR = i;
        USART1_IRQHandler();
    }
    CHECK(!CarUart_Read(&byte, &lost));
    CHECK(lost);
    CHECK(!CarUart_Read(&byte, &lost));
    CHECK(!lost);
    USART1->SR = USART_SR_RXNE | USART_SR_ORE;
    USART1->DR = 0xAA;
    USART1_IRQHandler();
    CHECK(!CarUart_Read(&byte, &lost));
    CHECK(lost);
    USART1->SR = USART_SR_RXNE;
    USART1->DR = 0x55;
    USART1_IRQHandler();
    CHECK(CarUart_Read(&byte, &lost));
    CHECK(!lost && byte == 0x55);
    return 0;
}
