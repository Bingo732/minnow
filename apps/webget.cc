#include "debug.hh"
// #include "socket.hh"
#include "tcp_minnow_socket.hh"

#include <cstdlib>
#include <format>
#include <iostream>
#include <span>
#include <string>

using namespace std;

namespace {
void get_URL( const string& host, const string& path )
{
  Address addr( host, "http" );
  // TCPSocket tcp_socket;
  CS144TCPSocket tcp_socket;
  tcp_socket.connect( addr );
  tcp_socket.write( format( "GET {} HTTP/1.1\r\nHost: {}\r\nConnection: close\r\n\r\n", path, host ) );
  tcp_socket.shutdown( 1 );
  string data;
  while ( !tcp_socket.eof() ) {
    tcp_socket.read( data );
    cout << data;
  }
  tcp_socket.shutdown( 0 );
  debug( "Function called: get_URL( \"{}\", \"{}\" )", host, path );
  debug( "get_URL() function not yet implemented" );
  tcp_socket.wait_until_closed();
}
} // namespace

int main( int argc, char* argv[] )
{
  try {
    if ( argc <= 0 ) {
      abort(); // For sticklers: don't try to access argv[0] if argc <= 0.
    }

    auto args = span( argv, argc );

    // The program takes two command-line arguments: the hostname and "path" part of the URL.
    // Print the usage message unless there are these two arguments (plus the program name
    // itself, so arg count = 3 in total).
    if ( argc != 3 ) {
      cerr << "Usage: " << args.front() << " HOST PATH\n";
      cerr << "\tExample: " << args.front() << " stanford.edu /class/cs144\n";
      return EXIT_FAILURE;
    }

    // Get the command-line arguments.
    const string host { args[1] };
    const string path { args[2] };

    // Call the student-written function.
    get_URL( host, path );
  } catch ( const exception& e ) {
    cerr << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
