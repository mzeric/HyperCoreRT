int c_main(void) {


	while(1);
}
__attribute__((naked, noreturn)) void _reset(void) {


	c_main();
}
