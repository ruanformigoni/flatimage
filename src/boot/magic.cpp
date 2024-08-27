//
// @author      : Ruan E. Formigoni (ruanformigoni@gmail.com)
// @file        : magic
// @created     : Monday Dec 25, 2023 23:37:09 -03
//
// @description : Patch magic bytes to identify file as a flatimage
//

#include <fstream>
#include <iostream>

int main(int argc, char** argv)
{
  // Check arg count
  if (argc != 2)
  {
    std::cerr << "Invalid parameters" << std::endl;
    return 1;
  }

  // Define magic
  unsigned char arr_magic[] = {'F', 'I', 0x01};

  // Open the file in binary mode for rw
  std::fstream file (argv[1], std::ios::in | std::ios::out | std::ios::binary);
  if (!file)
  {
    std::cerr << "Error opening file" << std::endl;
    return -1;
  }

  // Seek
  file.seekp(8);

  // Check for seek errors
  if (!file)
  {
    std::cerr << "Error seeking position" << std::endl;
    file.close();
    return -1;
  }

  // Write the new bytes to the specified position
  file.write(reinterpret_cast<const char*>(arr_magic), sizeof(arr_magic));

  // Check for errors after writing
  if (!file)
  {
    std::cerr << "Error writing to file" << std::endl;
    file.close();
    return -1;
  }

  // Close the file
  file.close();
  return 0;
}

// cmd: !g++ %:p -o %:p:r
/* vim: set expandtab fdm=marker ts=2 sw=2 tw=100 et :*/
