#ifndef STRING2IP_H
#define STRING2IP_H

#include "iptools.h"
/* read len chars from 's' and try to convert to a struct ip_addr
 * s doesnt need to be '\0' ended
 * returns:
 *    IPV4_A if addr is valid IPv4
 *    IPV6_A if addr is valid IPv6
 *    BAD_IP otherwise
 **/
int string2addr(const char *s, struct ip_addr *addr, size_t len);

/* read len chars from 's' and try to convert to a subnet mask length
 * s doesnt need to be '\0' ended
 * returns :
 *    'mask length' is 's' is a valid mask
 *     BAD_MASK otherwise
*/
int string2mask(const char *s, size_t len);


/* get_subnet_or_ip: fills struct subnet * from a string
 * @s      : the string to read
 * @subnet : a pointer to a struct subnet to fill
 * returns:
 *    IPV4_A : IPv4 without mask
 *    IPV4_N : IPv4 + mask
 *    IPV6_A : IPv6 without mask
 *    IPV6_N : IPv6 +  mask
 *    BAD_IP, BAD_MASK on error
 */
int get_subnet_or_ip(const char *s, struct subnet *subnet);

/* variant of get_subnet_or_ip that allows IPv4 address to not print :
 * 'useless 0' like 10/8, 172.20/16, 192.168.1/24
 * or that, if not mask is present, will use the classfull mask :
 * 192.168.1.0 will translate in 192.168.1.0/24
 * Classfull_get_subnet always creates subnet+mask,
 * while get_subnet_or_ip create IPv4/32 or IPv6/128
 * in case no mask is found
 **/
int classfull_get_subnet(const char *string, struct subnet *subnet);

#else
#endif
