#include "../include/bitcompressor.hpp"

#include "7zip/Archive/IArchive.h"
#include "Windows/COM.h"
#include "Windows/FileFind.h"
#include "Windows/PropVariant.h"

#include "../include/updatecallback.hpp"
#include "../include/bitexception.hpp"

using namespace Bit7z;
using namespace NWindows;

BitCompressor::BitCompressor( const Bit7zLibrary& lib, BitFormat format ) : mLibrary( lib ),
    mFormat( format ), mPassword( L"" ), mCryptHeaders( false ), mSolidMode( false ) {}

void BitCompressor::setPassword( const wstring& password, bool crypt_headers ) {
    mPassword = password;
    mCryptHeaders = crypt_headers;
}

void BitCompressor::compress( const vector<wstring>& in_files, const wstring& out_archive ) {
    vector<FSItem> dirItems;
    for ( wstring filePath : in_files ) {
        FSItem item( filePath );
        if ( ! item.exists() ) throw BitException( L"Item '" + item.name() + L"' does not exists" );
        if ( item.isDir() ) {
            FSIndexer indexer( filePath );
            indexer.listFilesInDirectory( dirItems );
        } else {
            dirItems.push_back( item );
        }
    }
    compressFS( dirItems, out_archive );
}

void BitCompressor::compressFile( const wstring& in_file, const wstring& out_archive ) {
    compressFiles( {in_file}, out_archive );
}

void BitCompressor::compressFiles( const vector<wstring>& in_files, const wstring& out_archive ) {
    vector<FSItem> dirItems;
    for ( wstring filePath : in_files ) {
        FSItem item( filePath );
        if ( item.exists() && !item.isDir() )
            dirItems.push_back( item );
    }
    compressFS( dirItems, out_archive );
}

void BitCompressor::compressDirectory( const wstring& in_dir, const wstring& out_archive,
                                       bool search_subdirs ) {
    vector<FSItem> dirItems;
    FSIndexer indexer( in_dir );
    indexer.listFilesInDirectory( dirItems, search_subdirs );
    compressFS( dirItems, out_archive );
}

void BitCompressor::compressFS( const vector<FSItem>& in_items, const wstring& out_archive ) {
    CMyComPtr<IOutArchive> outArchive = mLibrary.outputArchiveObject( mFormat );
    if ( mCryptHeaders ) {
        const wchar_t* names[] = {L"he"};
        const int kNumProps = sizeof( names ) / sizeof( names[0] );
        NWindows::NCOM::CPropVariant values[kNumProps] = {
            true     // crypted headers ON
        };
        CMyComPtr<ISetProperties> setProperties;
        if ( outArchive->QueryInterface( IID_ISetProperties, ( void** )&setProperties ) != S_OK )
            throw BitException( "ISetProperties unsupported" );
        if ( setProperties->SetProperties( names, values, kNumProps ) != S_OK )
            throw BitException( "Cannot set properties of the archive" );
    }
    COutFileStream* outFileStreamSpec = new COutFileStream;
    if ( !outFileStreamSpec->Create( out_archive.c_str(), false ) )
        throw BitException( "Can't create archive file" );
    UpdateCallback* updateCallbackSpec = new UpdateCallback( in_items );
    updateCallbackSpec->setPassword( mPassword );
    CMyComPtr<IArchiveUpdateCallback2> updateCallback( updateCallbackSpec );
    HRESULT result = outArchive->UpdateItems( outFileStreamSpec, ( UInt32 )in_items.size(),
                                              updateCallback );
    updateCallbackSpec->Finilize();

    if ( result != S_OK ) throw BitException( updateCallbackSpec->getErrorMessage() );

    wstring errorString = L"Error for files: ";
    for ( unsigned int i = 0; i < updateCallbackSpec->mFailedFiles.size(); i++ )
        errorString += updateCallbackSpec->mFailedFiles[i] + L" ";

    if ( updateCallbackSpec->mFailedFiles.size() != 0 )
        throw BitException( errorString );
}
