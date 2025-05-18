//============================================================================
//
//   SSSS    tt          lll  lll       
//  SS  SS   tt           ll   ll        
//  SS     tttttt  eeee   ll   ll   aaaa 
//   SSSS    tt   ee  ee  ll   ll      aa
//      SS   tt   eeeeee  ll   ll   aaaaa  --  "An Atari 2600 VCS Emulator"
//  SS  SS   tt   ee      ll   ll  aa  aa
//   SSSS     ttt  eeeee llll llll  aaaaa
//
// Copyright (c) 1995-2014 by Bradford W. Mott, Stephen Anthony
// and the Stella Team
//
// See the file "License.txt" for information on usage and redistribution of
// this file, and for a DISCLAIMER OF ALL WARRANTIES.
//
// $Id: Serializer.cxx 2838 2014-01-17 23:34:03Z stephena $
//============================================================================

#include "Serializer.hxx"

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Serializer::Serializer(const string& filename, bool readonly)
  : myFile(NULL),
    myReadOnly(readonly)
{
  if(readonly)
  {
    myFile = fopen(filename.c_str(), "rb");
  }
  else
  {
    // Open in read/write mode, create if doesn't exist
    myFile = fopen(filename.c_str(), "rb+");
    if(!myFile)
    {
      // If file doesn't exist, create it
      myFile = fopen(filename.c_str(), "wb+");
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
Serializer::~Serializer(void)
{
  if(myFile)
  {
    fclose(myFile);
    myFile = NULL;
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Serializer::isValid(void)
{
  return myFile != NULL;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Serializer::reset(void)
{
  if(myFile)
  {
    rewind(myFile);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt8 Serializer::getByte(void)
{
  uInt8 val = 0;
  if(myFile)
  {
    fread(&val, 1, 1, myFile);
  }
  return val;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Serializer::getByteArray(uInt8* array, uInt32 size)
{
  if(myFile)
  {
    fread(array, 1, size, myFile);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt16 Serializer::getShort(void)
{
  uInt16 val = 0;
  if(myFile)
  {
    fread(&val, sizeof(uInt16), 1, myFile);
  }
  return val;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Serializer::getShortArray(uInt16* array, uInt32 size)
{
  if(myFile)
  {
    fread(array, sizeof(uInt16), size, myFile);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
uInt32 Serializer::getInt(void)
{
  uInt32 val = 0;
  if(myFile)
  {
    fread(&val, sizeof(uInt32), 1, myFile);
  }
  return val;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Serializer::getIntArray(uInt32* array, uInt32 size)
{
  if(myFile)
  {
    fread(array, sizeof(uInt32), size, myFile);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
string Serializer::getString(void)
{
  int len = getInt();
  string str;
  str.resize(len);
  if(myFile)
  {
    fread(&str[0], 1, len, myFile);
  }
  return str;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
bool Serializer::getBool(void)
{
  return getByte() == TruePattern;
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Serializer::putByte(uInt8 value)
{
  if(myFile && !myReadOnly)
  {
    fwrite(&value, 1, 1, myFile);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Serializer::putByteArray(const uInt8* array, uInt32 size)
{
  if(myFile && !myReadOnly)
  {
    fwrite(array, 1, size, myFile);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Serializer::putShort(uInt16 value)
{
  if(myFile && !myReadOnly)
  {
    fwrite(&value, sizeof(uInt16), 1, myFile);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Serializer::putShortArray(const uInt16* array, uInt32 size)
{
  if(myFile && !myReadOnly)
  {
    fwrite(array, sizeof(uInt16), size, myFile);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Serializer::putInt(uInt32 value)
{
  if(myFile && !myReadOnly)
  {
    fwrite(&value, sizeof(uInt32), 1, myFile);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Serializer::putIntArray(const uInt32* array, uInt32 size)
{
  if(myFile && !myReadOnly)
  {
    fwrite(array, sizeof(uInt32), size, myFile);
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Serializer::putString(const string& str)
{
  if(!myReadOnly)
  {
    uInt32 len = str.length();
    putInt(len);
    if(myFile)
    {
      fwrite(str.data(), 1, len, myFile);
    }
  }
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
void Serializer::putBool(bool b)
{
  putByte(b ? TruePattern: FalsePattern);
}
