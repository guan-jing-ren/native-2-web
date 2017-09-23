_This project is licenced under GPLv3. For a commercial licence, email me to discuss._

# native-2-web

You have a function (libraries of functions) written in C++ and you want it to be called from a web environment. You want the client-side function call to look like the server-side function. native-2-web does this for you.

With native-2-web, you can do away with unwieldy Java EE or .NET runtimes, overbearing "frameworks", excessive annotations, and generating intermediate processed files. Let the compiler do the heavy lifting. Don't let the middle~~man~~ware dictate your design by cutting out the middle~~man~~ware. Design and write your server side application in a way that _feels right_, and drive it from the web the way you would drive it as a standalone program.

Thus, native-2-web aims to simplify the creation of web-based applications.

It removes the need to design any RESTful url schemes, any JSON or XML data transport, and transformation between the transport and the language data types. Just call your native C++ function from the web just as you would call any C++ function in a C++ program. Doing so will also decouple your front end from the business logic. You have the cross platform power of HTML5, WebGL, Canvas and SVG to do all you need on the client side.

## Top level design
There are four components to this library.

The three core components implementing the backend are:

- Data and API mangling.
- Serialization and deserialization.
- API registration.

One auxiliary component that links between the web front end and the native backend:

- Javascript generator for calling APIs.
- Javascript UI skeleton generator.

And one auxiliary component for serving APIs:

- A HTTP and websocket client and server.

## Usages

## Design rationale

The three component groups are designed to be orthogonal to each other to the extent that they could well be standalone libraries in their own right. For instance, an HTTP and websocket server implementation based on Boost.ASIO and Beast is provided, but the core and Javascript components are designed to be agnostic to any server implementation.

The Javascript and serialization components operate on a shared data marshalling scheme, but said scheme is based on the concept behind the specific types rather than dependent on the actual types.

### Data marshalling concepts and scheme

Data is considered to fall into the four categories, with the following marshalling method:

1. Fundamental, non-pointer types. These include integral types, floating point types and enum types.
   1. Serialized as is with no padding. For Javascript compatibility, no 64-bit integer support.
1. Sequential containers. These include vectors, lists and sets.
   1. For containers with sizes fixed at compile time, data is serialized in contiguous memory from beginning to end.
   1. For containers with dynamic size, the data is serialized as previous, but preceded by a 32-bit length field.
1. Associative containers. These include maps and unordered_maps.
   1. A 32-bit length field, followed by keys, followed by values. The length field is applicable to both.
1. Heterogenous containers. Pairs, tuples and structures.
   1. Members are serialized in declaration order with no padding.

Compound types are only acceptable if they contain only data from the above categories. ie, compound types can contain compound types, as long as the nesting is terminated by any of fundamental, non-pointer types.

Due to the lack of compile-time reflection in C++, supporting custom data structures requires, as a maximum, annotation of the salient data members of the structure. The annotations are simple macros; no processing step to generate intermediate files is required.

### Front end Javascript

The generated Javascript to call the native API (via websocket) should resemble the native API argument-for-argument, with an extra argument specifying the websocket send the remote call to, and to receive the result of the API. Fundamental, non-pointer types are passed as they are. Sequential containers are passed as arrays. Associative and heterogenous containers are passed as Javascript objects.

### Server implementation

The server is designed around extreme SFINAE for extensibility. There are no interfaces that is required to be inherited from. There are no traits classes to specialize. The server detects capabilities at compile time and SFINAE enables only the mininum feature set. eg, if a server extension implements a websocket handler, but not an HTTP handler, the resulting connection instantiation only handles websocket code paths. eg, if a websocket handler only handles binary messages, only binary message code paths are SFINAE enabled. The minimum server extension implementation allowed is an empty class, which does nothing.

For a server side connection, the user does not even get a server object. The server object lifetime is managed by the Boost.ASIO io_service, and all customized handling is done through the provided server extension. For a client side connection, the user does get a connection object that only allows sending requests. All other concerns are handled within the implementation, invisible to the user.
