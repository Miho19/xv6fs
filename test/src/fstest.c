#include <stdlib.h>
#include <stdio.h>
#include <check.h>
#include <check_stdint.h>
#include "fs.h"

FILE *f;
struct io_ops io;

START_TEST(open_f) {
    ck_assert_ptr_ne(f, 0);
} END_TEST


START_TEST(read_sector_1) {
    int result = 0;
    unsigned char buffer[BSIZE];
    memset(buffer, 0, sizeof(buffer));
    result = rsec(1, buffer);
    ck_assert_int_eq(result, BSIZE);

} END_TEST

START_TEST(read_sector_failure) {
    int result = 0;
    unsigned char buffer[BSIZE];
    memset(buffer, 0, sizeof(buffer));
    result = rsec(-1, buffer);
    ck_assert_int_eq(result, 0);
} END_TEST

START_TEST(superblock_read_test) {
    struct superblock *s;
    
    unsigned char buffer[BSIZE];
    memset(buffer, 0, sizeof(buffer));

    rsec(1, buffer);
    
    s = (struct superblock *)buffer;
    ck_assert_int_eq(s->size, 1024);

} END_TEST

START_TEST(disk_root_inode_read) {
    
    unsigned char buffer[BSIZE];
    struct dinode *diptr;
    uint inum = 1;
    memset(buffer, 0, sizeof(buffer));

    rsec(IBLOCK(inum), buffer);

    diptr = (struct dinode *)buffer + (inum % IPB);

    ck_assert_int_eq(diptr->size, BSIZE);
    ck_assert_int_eq(diptr->type, T_DIR);

} END_TEST

START_TEST(get_root_inode){
    struct inode ip;
    int result = 0;
    memset(&ip, 0, sizeof ip);
    result = iget(1, &ip);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(ip.inum, 1);    

} END_TEST

START_TEST(get_inode_failure) {
    struct superblock sb;
    struct inode ip;
    int result = 0;

    memset(&sb, 0, sizeof sb);
    memset(&ip, 0, sizeof ip);
    readsb(&sb);

    result = iget(sb.ninodes + 1, &ip);

    ck_assert_int_eq(result, 1);


} END_TEST

START_TEST(get_second_inode){
    struct inode ip;
    int result = 0;
    memset(&ip, 0, sizeof ip);
    result = iget(2, &ip);

    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(ip.inum, 2);   
}END_TEST

/** 
 * Create a file, link to parent with name `hello.txt`, 
 * write `Hello World!` to file, retrieve that file from parent directory
 * check its contents
*/

START_TEST(add_new_file){
    struct inode ip;
    struct inode testip;
    int result = 0;
    char name[DIRSIZ];
    char name_test[DIRSIZ];
    unsigned char data[] = "Hello World!";
    unsigned char buffer[BSIZE];

    memset(&ip, 0, sizeof ip);
    memset(name, 0, sizeof name);
    strncpy(name, "hello.txt", DIRSIZ);

    result = ialloc(&ip, T_FILE);
    ck_assert_int_eq(result, 0);

    ck_assert_int_eq(ip.size, 0);
    ck_assert_int_eq(ip.type, T_FILE);
    ck_assert_int_eq(ip.nlink, 1);

    result = ilink(1, name, &ip);
    ck_assert_int_eq(result, 0);

    result = iwrite(&ip, data, strlen((char *)data), 0);
    ck_assert_int_eq(result, strlen((char *)data));

    memset(&testip, 0, sizeof testip);
    result = nparent(1, name, &testip, 0);
    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(testip.inum, ip.inum);

    memset(name_test, 0, sizeof name_test);
    memset(&testip, 0, sizeof testip);
    result = iparent(1, ip.inum, name_test, &testip, 0);
    ck_assert_int_eq(result, 0);
    ck_assert_int_eq(testip.inum, ip.inum);
    ck_assert_int_eq(strncmp((char *)name, (char *)name_test, DIRSIZ), 0);

    memset(buffer, 0, sizeof buffer);
    result = iread(&ip, buffer, strlen((char *)data), 0);
    ck_assert_int_eq(result, strlen((char *)data));
    ck_assert_int_eq(strncmp((char *)data, (char *)buffer, DIRSIZ), 0);

} END_TEST


Suite* fs_suite(void) {
    Suite *s;
    TCase *tr_core;

    s = suite_create("File System");

    tr_core = tcase_create("Core");

    tcase_add_test(tr_core, open_f);
    tcase_add_test(tr_core, read_sector_1);
    tcase_add_test(tr_core, read_sector_failure);
    tcase_add_test(tr_core, superblock_read_test);
    tcase_add_test(tr_core, disk_root_inode_read);
    tcase_add_test(tr_core, get_root_inode);
    tcase_add_test(tr_core, get_inode_failure);
    tcase_add_test(tr_core, get_second_inode);
    tcase_add_test(tr_core, add_new_file);
    
    suite_add_tcase(s, tr_core);
    return s;
}


int main(void) {
    int number_failed = 0;
    Suite *s;
    SRunner *sr;


    f = fopen("fs.img", "rb+");

    io.f = f;
    io.rsec = 0;
    io.wsec = 0;

    s = fs_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);

    srunner_free(sr);

    return (number_failed == 0 ) ? EXIT_SUCCESS : EXIT_FAILURE;
}