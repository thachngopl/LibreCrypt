// Description: 
// By Sarah Dean
// Email: sdean12@sdean12.org
// WWW:   http://www.FreeOTFE.org/
//
// -----------------------------------------------------------------------------
//

#include "FreeOTFEPlatform.h"

#ifdef FOTFE_PC_DRIVER
#include <ntddk.h>
#endif

#include "FreeOTFECypherCAST5.h"

#include "FreeOTFECypherImpl.h"
#include "FreeOTFECypherAPICommon.h"

#ifdef FOTFE_PC_DRIVER
#include "FreeOTFECypherDriver.h"  // Required for DRIVER_CYPHER_VERSION
#else
#include "FreeOTFE4PDACypherDriver.h"  // Required for DRIVER_CYPHER_VERSION
#endif

#include "FreeOTFEDebug.h"
#include "FreeOTFElib.h"


// Include libtomcrypt library...
#include <tomcrypt.h>
#include <tomcrypt_custom.h>


// =========================================================================
// Cypher driver init function
// driverInfo - The structure to be initialized
NTSTATUS
ImpCypherDriverExtDetailsInit_v3(
    IN OUT CYPHER_DRIVER_INFO_v3* driverInfo
)
{
    NTSTATUS status = STATUS_SUCCESS;
    int idx = -1;

    DEBUGOUTCYPHERIMPL(DEBUGLEV_ENTER, (TEXT("ImpCypherDriverExtDetailsInit_v3\n")));


    // -- POPULATE DRIVER IDENTIFICATION --
    FREEOTFE_MEMZERO(driverInfo->DriverTitle, sizeof(driverInfo->DriverTitle));
    FREEOTFE_MEMCPY(
                  driverInfo->DriverTitle,
                  DRIVER_TITLE,
                  strlen(DRIVER_TITLE)
                 );

    driverInfo->DriverGUID = DRIVER_GUID;
    driverInfo->DriverVersionID = DRIVER_CYPHER_VERSION;


    // -- POPULATE CYPHERS SUPPORTED --
    driverInfo->CypherCount = CYPHERS_SUPPORTED;
    driverInfo->CypherDetails = FREEOTFE_MEMCALLOC(
                                                   sizeof(CYPHER_v3),
                                                   driverInfo->CypherCount
                                                  );


    // -- CAST5 --
    idx++;
    FREEOTFE_MEMZERO(driverInfo->CypherDetails[idx].Title, MAX_CYPHER_TITLE);
    FREEOTFE_MEMCPY(
                  driverInfo->CypherDetails[idx].Title,
                  DRIVER_CIPHER_TITLE_CAST5,
                  strlen(DRIVER_CIPHER_TITLE_CAST5)
                 );

    driverInfo->CypherDetails[idx].Mode              = CYPHER_MODE_CBC;
    driverInfo->CypherDetails[idx].KeySizeUnderlying = 128;
    driverInfo->CypherDetails[idx].BlockSize         = (cast5_desc.block_length * 8);
    driverInfo->CypherDetails[idx].VersionID         = DRIVER_CYPHER_IMPL_VERSION;
    driverInfo->CypherDetails[idx].CypherGUID        = CIPHER_GUID_CAST5;
    driverInfo->CypherDetails[idx].KeySizeRequired   = driverInfo->CypherDetails[idx].KeySizeUnderlying;


    DEBUGOUTCYPHERIMPL(DEBUGLEV_EXIT, (TEXT("ImpCypherDriverExtDetailsInit_v3\n")));

    return status;
}


// =========================================================================
// Cypher driver cleardown function
// driverInfo - The structure to be cleared down
NTSTATUS
ImpCypherDriverExtDetailsCleardown_v3(    
    IN OUT CYPHER_DRIVER_INFO_v3* driverInfo
)
{
    NTSTATUS status = STATUS_SUCCESS;

    DEBUGOUTCYPHERIMPL(DEBUGLEV_ENTER, (TEXT("ImpCypherDriverExtDetailsCleardown_v3\n")));

    if (driverInfo->CypherDetails != NULL)
        {
        FREEOTFE_FREE(driverInfo->CypherDetails);
        }

    driverInfo->CypherDetails = NULL;
    driverInfo->CypherCount = 0;

    DEBUGOUTCYPHERIMPL(DEBUGLEV_EXIT, (TEXT("ImpCypherDriverExtDetailsCleardown_v3\n")));

    return status;
}


// =========================================================================
// Encryption function
// Note: CyphertextLength must be set to the size of the CyphertextData buffer on
//       entry; on exit, this will be set to the size of the buffer used.
NTSTATUS
ImpCypherEncryptSectorData(
    IN      GUID* CypherGUID,
    IN      LARGE_INTEGER SectorID,  // Indexed from zero
    IN      int SectorSize, // In bytes
    IN      int KeyLength,  // In bits
    IN      FREEOTFEBYTE* Key,
    IN      char* KeyASCII,  // ASCII representation of "Key"
    IN      int IVLength,  // In bits
    IN      FREEOTFEBYTE* IV,
    IN      int PlaintextLength,  // In bytes
    IN      FREEOTFEBYTE* PlaintextData,
    OUT     FREEOTFEBYTE* CyphertextData
)
{
    NTSTATUS status = STATUS_SUCCESS;
    // libtomcrypt can't handle NULL IVs in CBC mode - it ASSERTs that IV != NULL
    char ltcNullIV[FREEOTFE_MAX_CYPHER_BLOCKSIZE];
    int cipher;
    symmetric_CBC *cbc;
    int errnum;

    DEBUGOUTCYPHERIMPL(DEBUGLEV_ENTER, (TEXT("ImpCypherEncryptData\n")));

    if (!(
		  (IsEqualGUID(&CIPHER_GUID_CAST5, CypherGUID))
		 ))
		{
		DEBUGOUTCYPHERIMPL(DEBUGLEV_ERROR, (TEXT("Unsupported cipher GUID passed in.\n")));
        status = STATUS_INVALID_PARAMETER;
        }

    // libtomcrypt can't handle NULL IVs in CBC mode - it ASSERTs that IV != NULL
    if ( (IVLength == 0) || (IV == NULL) )
        {
        FREEOTFE_MEMZERO(&ltcNullIV, sizeof(ltcNullIV));
        IV = (char*)&ltcNullIV;
        }

    cbc = FREEOTFE_MEMALLOC(sizeof(symmetric_CBC));    
    FREEOTFE_MEMZERO(cbc, sizeof(symmetric_CBC));

	  if NT_SUCCESS(status) 
		    {
		    status = InitLTCCypher(&cipher);
		    }

    if NT_SUCCESS(status)
        {
        // Start a CBC session
        if ((errnum = cbc_start(
                                cipher, 
                                IV, 
                                Key, 
                                (KeyLength/8), 
                                0, 
                                cbc
                               )) != CRYPT_OK)
            {
            status = STATUS_UNSUCCESSFUL;
            DEBUGOUTCYPHERIMPL(DEBUGLEV_ERROR, (TEXT("Unable to start CBC session (errnum: %d)\n"), errnum));
            }
        else
            {
            if ((errnum = cbc_encrypt(
                                      PlaintextData, 
                                      CyphertextData, 
                                      PlaintextLength, 
                                      cbc
                                     )) != CRYPT_OK)
                {
                DEBUGOUTCYPHERIMPL(DEBUGLEV_ERROR, (TEXT("Unable to encrypt/decrypt block (errnum: %d)\n"), errnum));
                status = STATUS_UNSUCCESSFUL;
                }

            cbc_done(cbc);
            }
        }

    SecZeroMemory(cbc, sizeof(symmetric_CBC));
    FREEOTFE_FREE(cbc);

    DEBUGOUTCYPHERIMPL(DEBUGLEV_EXIT, (TEXT("ImpCypherEncryptData\n")));

    return status;
}


// =========================================================================
// Decryption function
// Note: PlaintextLength must be set to the size of the PlaintextData buffer on
//       entry; on exit, this will be set to the size of the buffer used.
NTSTATUS
ImpCypherDecryptSectorData(
    IN      GUID* CypherGUID,
    IN      LARGE_INTEGER SectorID,  // Indexed from zero
    IN      int SectorSize, // In bytes
    IN      int KeyLength,  // In bits
    IN      FREEOTFEBYTE* Key,
    IN      char* KeyASCII,  // ASCII representation of "Key"
    IN      int IVLength,  // In bits
    IN      FREEOTFEBYTE* IV,
    IN      int CyphertextLength,  // In bytes
    IN      FREEOTFEBYTE* CyphertextData,
    OUT     FREEOTFEBYTE* PlaintextData
)
{
    NTSTATUS status = STATUS_SUCCESS;
    // libtomcrypt can't handle NULL IVs in CBC mode - it ASSERTs that IV != NULL
    char ltcNullIV[FREEOTFE_MAX_CYPHER_BLOCKSIZE];
    int cipher;
    symmetric_CBC *cbc;
    int errnum;

    DEBUGOUTCYPHERIMPL(DEBUGLEV_ENTER, (TEXT("ImpCypherDecryptData\n")));

	if (!(
		  (IsEqualGUID(&CIPHER_GUID_CAST5, CypherGUID))
		 ))
		{
		DEBUGOUTCYPHERIMPL(DEBUGLEV_ERROR, (TEXT("Unsupported cipher GUID passed in.\n")));
        status = STATUS_INVALID_PARAMETER;
        }

    // libtomcrypt can't handle NULL IVs in CBC mode - it ASSERTs that IV != NULL
    if ( (IVLength == 0) || (IV == NULL) )
        {
        FREEOTFE_MEMZERO(&ltcNullIV, sizeof(ltcNullIV));
        IV = (char*)&ltcNullIV;
        }

    cbc = FREEOTFE_MEMALLOC(sizeof(symmetric_CBC));    
    FREEOTFE_MEMZERO(cbc, sizeof(symmetric_CBC));

  	if NT_SUCCESS(status) 
    		{
	    	status = InitLTCCypher(&cipher);
  		  }

    if NT_SUCCESS(status)
        {
        // Start a CBC session
        if ((errnum = cbc_start(
                                cipher, 
                                IV, 
                                Key, 
                                (KeyLength/8), 
                                0, 
                                cbc
                               )) != CRYPT_OK)
            {
            status = STATUS_UNSUCCESSFUL;
            DEBUGOUTCYPHERIMPL(DEBUGLEV_ERROR, (TEXT("Unable to start CBC session (errnum: %d)\n"), errnum));
            }
        else
            {
            if ((errnum = cbc_decrypt(
                                      CyphertextData, 
                                      PlaintextData, 
                                      CyphertextLength, 
                                      cbc
                                     )) != CRYPT_OK)
                {
                DEBUGOUTCYPHERIMPL(DEBUGLEV_ERROR, (TEXT("Unable to encrypt/decrypt block (errnum: %d)\n"), errnum));
                status = STATUS_UNSUCCESSFUL;
                }

            cbc_done(cbc);
            }
        }

    SecZeroMemory(cbc, sizeof(symmetric_CBC));
    FREEOTFE_FREE(cbc);

    DEBUGOUTCYPHERIMPL(DEBUGLEV_EXIT, (TEXT("ImpCypherDecryptData\n")));

    return status;
}


// =========================================================================
// Initialize libtomcrypt cypher
NTSTATUS
InitLTCCypher(
    OUT  int *cipher
)
{
	  NTSTATUS status = STATUS_CRYPTO_SYSTEM_INVALID;

    DEBUGOUTCYPHERIMPL(DEBUGLEV_ENTER, (TEXT("InitLTCCypher\n")));

    // Initialize cipher
    *cipher = register_cipher(&cast5_desc);
    if (*cipher == -1)
        {
	    DEBUGOUTCYPHERIMPL(DEBUGLEV_ERROR, (TEXT("Could not register cipher\n")));
        }
    else
        {    
        *cipher = find_cipher("cast5");
        if (*cipher == -1)
            {
      	    DEBUGOUTCYPHERIMPL(DEBUGLEV_ERROR, (TEXT("Could not find cipher\n")));
            }
		else
			{
			status = STATUS_SUCCESS;
			}
        }

    DEBUGOUTCYPHERIMPL(DEBUGLEV_EXIT, (TEXT("InitLTCCypher\n")));

	return status;
}


// =========================================================================
// =========================================================================

