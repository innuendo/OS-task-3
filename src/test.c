/*
 *  test.c
 *  Z3
 *
 *  Created by Krzysztof Wiśniewski on 11-01-11.
 *  Copyright 2011 Uniwersytet Warszawski. All rights reserved.
 *
 */


#include <stdio.h>
#include "pagesim.h"

void callback(int op, int a1, int a2) {
	printf("op = %d, a1 = %d, a2 = %d", op, a1, a2);
}
int main() {
	page_sim_init(4, 32, 128, 8, &callback);
	return 0;
}