/** @file
 * @brief Test Environment: 
 *
 * Traffic Application Domain Command Handler
 * SNMP protocol implementaion internal declarations.
 *
 * Copyright (C) 2003 Test Environment authors (see file AUTHORS in the
 * root directory of the distribution).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 *
 * @author Renata Sayakhova <Renata.Sayakhova@oktetlabs.ru>
 *
 * $Id: $
 */
#ifndef __TE_TAPI_SNMP_MACRO_H__
#define __TE_TAPI_SNMP_MACRO_H__ 

#include "tapi_snmp.h"
#include "tapi_test.h"

#ifdef __cplusplus
extern "C" {
#endif


/**
 * Macro around tapi_snmp_csap_create().
 * 
 * @param ta            Test Agent name.
 * @param sid           RCF Session ID.
 * @param snmp_agent    Address of SNMP agent.
 * @param community     SNMP community.
 * @param snmp_version  SNMP version.
 * @param csap_id       identifier of an SNMP CSAP. (OUT)
 * 
 */
#define SNMP_CSAP_CREATE(ta_, sid_, snmp_agent_, community_, snmp_version_, csap_id_) \
    do                                                                                \
    {                                                                                 \
        int rc_;                                                                      \
	                                                                              \
        rc_ = tapi_snmp_csap_create(ta_, sid_, snmp_agent_, community_,               \
                                    snmp_version_, &csap_id_);                        \
        if (rc_ != 0)                                                                 \
	{                                                                             \
            TEST_FAIL("snmp csap creation failed %X\n", rc_);                         \
        }                                                                             \
    } while (0)

/**
 * Macro around tapi_snmp_make_oid().
 * 
 * @param lable_    SNMP label - OID string representation
 * @param oid_      Location for parsed OID (OUT)
 * 
 */ 
#define SNMP_MAKE_OID(lable_, oid_)                                                   \
    do                                                                                \
    {                    	                                                      \
        int rc_;                                                                      \
	                                                                              \
        rc_ = tapi_snmp_make_oid(lable_, &oid);                                       \
	if (rc_ != 0)                                                                 \
	{                                                                             \
            TEST_FAIL("snmp make integer failed for OID %s, result %X\n",             \
		      lable, rc_);		                                      \
        }                                                                             \
    } while (0)	    
	

/**
 * Macro around tapi_snmp_set_integer().
 * 
 * @param ta_            Test Agent name
 * @param sid_           RCF Session id.
 * @param csap_id_       identifier of an SNMP CSAP.
 * @param lable_         SNMP lable - OID string representation.
 * @param value_         integer value.
 * 
 */
#define SNMP_SET_INTEGER(ta_, sid_, csap_id_, label_, value_)              \
    do                                                                     \  
    {                                                                      \
        int             rc_;                                               \
        int             errstat_;	                                   \
        tapi_snmp_oid_t oid_;                                              \
	                                                                   \
        SNMP_MAKE_OID(lable_, oid_);                                       \
 	rc_ = tapi_snmp_set_integer(ta_, sid_, csap_id_, &oid_,            \
			            value_, &errstat_);                    \
	if (rc_ != 0)                                                      \
	{                                                                  \
            TEST_FAIL("snmp set integer failed, result %X, errstat %X\n",  \
		       rc_, errstat_);                                     \
        }                                                                  \
    } while (0)        	    

/**
 * Macro around tapi_snmp_set_octetstring()
 * 
 * @param ta_            Test Agent name
 * @param sid_           RCF Session id.
 * @param csap_id_       identifier of an SNMP CSAP.
 * @param lable_         ID of an SNMP object the value is to be set.
 * @param value          octet string value.
 * @param size           octet string size.
 * 
 */
#define SNMP_SET_OCTETSTRING(ta_, sid_, csap_id_, lable_, value_, size_)      \
    do                                                                        \
    {                                                                         \	
        int             rc_;                                                  \
        int             errstat_;                                             \
	                                                                      \
        tapi_snmp_oid_t oid_;                                                 \
        SNMP_MAKE_OID(lable_, oid_);                                          \
 	rc_ = tapi_snmp_set_octetstring(ta_, sid_, csap_id_, &oid_,           \
			                value_, size_, &errstat_);            \
	if (rc_ != 0)                                                         \
	{                                                                     \
            TEST_FAIL("snmp set octetstring failed, result %X, errstat %X\n", \
	              rc_, errstat_);                                         \
        }                                                                     \
    } while (0)        	    


/**
 * Macro around tapi_snmp_set_string().
 * 
 * @param ta_            Test Agent name
 * @param sid_           RCF Session id.
 * @param csap_id_       identifier of an SNMP CSAP.
 * @param lable_         ID of an SNMP object the value is to be set.
 * @param value_         zero-terminated string.
 * 
 */
#define SNMP_SET_STRING(ta_, sid_, csap_id_, lable_, value_)               \
    do                                                                     \
    {                                                                      \	
        int             rc_;                                               \
        int             errstat_;                                          \
        tapi_snmp_oid_t oid_;                                              \
	                                                                   \
        SNMP_MAKE_OID(lable_, oid_);                                       \
 	rc_ = tapi_snmp_set_string(ta_, csid_, sap_id_, &oid_,             \
			                value_, &errstat_);                \
	if (rc_ != 0)                                                      \
	{                                                                  \
            TEST_FAIL("snmp set string failed, result %X, errstat %X\n",   \
	              rc_, errstat_);                                      \
        }                                                                  \
    } while (0)        	    


/**
 * Macro around tapi_snmp_walk().
 *
 * @param ta_            Test Agent name
 * @param sid_           RCF Session id.
 * @param csap_id_       identifier of an SNMP CSAP.
 * @param lable_         OID which defines a subtree to work with.
 * @param userdata_       opaque data to be passed into the callback function.
 * @param callback_       callback function, which is executed for each leaf.
 * 
 */
#define SNMP_WALK(ta_, sid_, csap_id_, lable_, userdata_, callback_)       \
    do                                                                     \
    {                                                                      \	
        int             rc_;                                               \
        tapi_snmp_oid_t oid_;                                              \
	                                                                   \
        SNMP_MAKE_OID(lable_, oid_);                                       \
 	rc_ = tapi_snmp_walk(ta_, sid_, csap_id_, &oid_, userdata_,        \
			     callback);                                    \
	if (rc_ != 0)                                                      \
	{                                                                  \
            TEST_FAIL("snmp walk failed, result %X\n", rc_);               \
        }                                                                  \
    } while (0)        	    


/**
 * Macro around tapi_snmp_get_ipaddr().
 *
 * @param ta_            Test Agent name
 * @param sid_           RCF Session id.
 * @param csap_id_       SNMP CSAP handle
 * @param lable_         ID of an SNMP object
 * @param addr_          Returned IPv4 address
 *                       Buffer must be sizeof(struct in_addr) bytes long
 *                       
 */
#define SNMP_GET_IPADDR(ta_, sid_, csap_id_, lable_, addr_)                \
    do                                                                     \
    {                                                                      \	
        int             rc_;                                               \
        tapi_snmp_oid_t oid_;                                              \
	                                                                   \
        SNMP_MAKE_OID(lable_, oid_);                                       \
 	rc_ = tapi_snmp_get_ipaddr(ta_, sid_, csap_id_, &oid_,             \
			           (void *)&addr_);                        \
	if (rc_ != 0)                                                      \
	{                                                                  \
            TEST_FAIL("snmp get ipaddr failed, result %X\n", rc_);         \
        }                                                                  \
    } while (0)        	    


/**
 * Macro around tapi_snmp_get_integer().
 *
 * @param ta_            Test Agent name
 * @param sid_           RCF Session id.
 * @param csap_id_       SNMP CSAP handle
 * @param oid_           ID of an SNMP object
 * @param val_           Returned value (OUT)
 * 
 */
#define SNMP_GET_INTEGER(ta_, sid_, csap_id_, lable_, val_)                \
    do                                                                     \
    {                                                                      \	
        int             rc_;                                               \
        tapi_snmp_oid_t oid_;                                              \
	                                                                   \
        SNMP_MAKE_OID(lable_, oid_);                                       \
 	rc_ = tapi_snmp_integer(ta_, sid_, csap_id_, &oid_, &val_);        \
	if (rc_ != 0)                                                      \
	{                                                                  \
            TEST_FAIL("snmp get integer failed, result %X\n", rc_);        \
        }                                                                  \
    } while (0)        	    

   
/**
 * Macro around tapi_snmp_get_string().
 *
 * @param ta_            Test Agent name
 * @param sid_           RCF Session id.
 * @param csap_id_       SNMP CSAP handle
 * @param lable_         ID of an SNMP object
 * @param buf_           Location for returned string (OUT)
 * @param buf_size_      Number of bytes in 'buf'
 *                       the number of bytes actually written on output (IN/OUT)
 *                       
 */
#define SNMP_GET_STRING(ta_, sid_, csap_id_, lable_, buf_, bufsize_)       \
    do                                                                     \
    {                                                                      \	
        int             rc_;                                               \
        tapi_snmp_oid_t oid_;                                              \
	                                                                   \
        SNMP_MAKE_OID(lable_, oid_);                                       \
 	rc_ = tapi_snmp_get_string(ta_, sid_, csap_id_, &oid_,             \
			           buf_, bufsize_);                        \
	if (rc_ != 0)                                                      \
	{                                                                  \
            TEST_FAIL("snmp get string failed, result %X\n", rc_);         \
        }                                                                  \
    } while (0)        	    

   
/**
 * Macro around tapi_snmp_get_octetstring().
 *
 * @param ta_            Test Agent name
 * @param sid_           RCF Session id.
 * @param csap_id_       SNMP CSAP handle
 * @param oid_           ID of an SNMP object
 * @param buf_           Location for returned value (OUT)
 * @param buf_size_      Number of bytes in 'buf' on input and 
 *                       the number of bytes actually written on output (IN/OUT)
 *                       
 */
#define SNMP_GET_OCTETSTRING(ta_, sid_, csap_id_, lable_, buf_, bufsize_)  \
    do                                                                     \
    {                                                                      \	
        int             rc_;                                               \
        tapi_snmp_oid_t oid_;                                              \
	                                                                   \
        SNMP_MAKE_OID(lable_, oid_);                                       \
 	rc_ = tapi_snmp_get_oct_string(ta_, sid_, csap_id_, &oid_,         \
			               buf_, bufsize_);                    \
	if (rc_ != 0)                                                      \
	{                                                                  \
            TEST_FAIL("snmp get octet string failed, result %X\n", rc_);   \
        }                                                                  \
    } while (0)        	    


/**
 * Macro around tapi_snmp_get_objid().
 *
 * @param ta_            Test Agent name
 * @param sid_           RCF Session id.
 * @param csap_id_       SNMP CSAP handle
 * @param lable_         ID of an SNMP object
 * @param ret_oid_       Returned value (OUT)
 *
 */
#define SNMP_GET_OBJID(ta_, sid_, csap_id_, lable_, ret_oid_)              \
    do                                                                     \
    {                                                                      \	
        int             rc_;                                               \
        tapi_snmp_oid_t oid_;                                              \
	                                                                   \
        SNMP_MAKE_OID(lable_, oid_);                                       \
 	rc_ = tapi_snmp_get_objid(ta_, sid_, csap_id_, &oid_, &ret_oid);   \
	if (rc_ != 0)                                                      \
	{                                                                  \
            TEST_FAIL("snmp get object id for %s failed, result %X\n",     \
                      lable_, rc_);                                        \
        }                                                                  \
    } while (0)        	    


/**
 * Macro around tapi_snmp_get_table().
 * 
 * @param ta_            Test Agent name
 * @param sid_           RCF Session id
 * @param csap_id_       SNMP CSAP handle
 * @param lable_         OID of SNMP table Entry object, or one leaf in this 
 *                       entry
 * @param num_           Number of raws in table = height of matrix below (OUT)
 * @param result_        Allocated matrix with results, if only 
 *                       one column should be get, matrix width is 1, otherwise 
 *                       matrix width is greatest subid of Table entry (OUT)
 *
 */
#define SNMP_GET_TABLE(ta_, sid_, csap_id_, lable_, num_, result_)         \
    do                                                                     \
    {                                                                      \	
        int             rc_;                                               \
        tapi_snmp_oid_t oid_;                                              \
	                                                                   \
        SNMP_MAKE_OID(lable_, oid_);                                       \
 	rc_ = tapi_snmp_get_table(ta_, sid_, csap_id_, &oid_,              \
                                  &num_, &result_);                        \
	if (rc_ != 0)                                                      \
	{                                                                  \
            TEST_FAIL("snmp get table for %s failed, result %X\n",         \
	              lable_, rc_);                                        \
        }                                                                  \
    } while (0)        	    

   
/**
 * Macro around tapi_snmp_get_table_rows().
 *
 * @param ta_            Test Agent name
 * @param sid_           RCF Session id
 * @param csap_id_       SNMP CSAP handle
 * @param lable_         OID of SNMP table Entry MIB node
 * @param num_           number of suffixes
 * @param suffixes_      Array with index suffixes of desired table rows
 * @param result_        Allocated matrix with results,
 *                       matrix width is greatest subid of Table entry (OUT)
 * 
 */
#define SNMP_GET_TABLE_ROWS(ta_, sid_, csap_id_, lable_, num_, suffixes_, result_) \
    do                                                                     \
    {                                                                      \	
        int             rc_;                                               \
        tapi_snmp_oid_t oid_;                                              \
	                                                                   \
        SNMP_MAKE_OID(lable_, oid_);                                       \
 	rc_ = tapi_snmp_get_table_rows(ta_, sid_, csap_id_, &oid_,         \
                                       num_, suffixes_, &result_);         \
	if (rc_ != 0)                                                      \
	{                                                                  \
            TEST_FAIL("snmp get table rows failed for %s failed, result %X\n",    \
	              lable_, rc_);                                        \
        }                                                                  \
    } while (0)        	    


/**
 * Macro around tapi_snmp_get_table_dimension().
 *
 * @param lable_     OID of SNMP table Entry object, or one leaf in this
 *                   entry
 * @param dimension  Table dimension
 *
 */ 
#define SNMP_GET_TABLE_DIMENSION(lable_, dimension_)                       \
    do                                                                     \
    {                                                                      \	
        int             rc_;                                               \
        tapi_snmp_oid_t oid_;                                              \
	                                                                   \
        SNMP_MAKE_OID(lable_, oid_);                                       \
 	rc_ = tapi_snmp_get_table_dimension(&oid_, &dimension_);           \
	if (rc_ != 0)                                                      \
	{                                                                  \
            TEST_FAIL("snmp get table dimension failed for %s failed, result %X\n",    \
	              lable_, rc_);                                        \
        }                                                                  \
    } while (0)        	    

   
/**
 * Macro around tapi_snmp_load_mib_with_path().
 *
 * @param dir_path_  Path to directory with MIB files
 * @param mib_file_  File name of the MIB file
 *
 */
#define SNMP_LOAD_MIB_WITH_PATH(dir_path_, file_)
    do                                                                     \
    {                                                                      \
       	int rc_;                                                           \
        rc_ = tapi_snmp_load_mib_with_path(dir_path_,                      \
			                   mib_file_);                     \
	if (rc_ != 0)                                                      \
        {                                                                  \
            TEST_FAIL("Loading mib with path failed, result %X\n", rc_);   \
        }                                                                  \
    } while (0)


/**
 * Macro around tapi_snmp_load_mib().
 *
 * @param mib_file_  File name of the MIB file.
 * 
 */
#define SNMP_LOAD_MIB_(file_)                                               \
    do                                                                      \
    {                                                                       \
       	int rc_;                                                            \
        rc_ = tapi_snmp_load_mib(mib_file_);                                \
	if (rc_ != 0)                                                       \
        {                                                                   \
            TEST_FAIL("Loading mib %s failed, result %X\n", mib_file_, rc_);\
        }                                                                   \
    } while (0)


#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* !__TE_TAPI_SNMP_MACRO_H__ */
