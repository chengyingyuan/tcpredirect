# TCPRedirect - Chaining tcp connections securely

## Rational

If you want to secure tcp connections across linux/windows platform without 
bothering ssh tunnel, this tool is for you.

## Compile

Under Windows, MS Visual Studio is needed to build, or you can use binary 
file.

Under Linux, all you need is typing 'make'.

## Usage

tcpredirect [-k [<left-key>]:[<right-key>]] 
	[-b [left-buffer-size]:[right-buffer-size]]
	[-t [left-timeout]:[right-timeout]]
	[<left-addr>]:<left-port>:[<right-addr>]:<right-port>

## Example

If you want to redirect office web traffic to a proxy server(www.example.com)
through a office server(office.example.com) with securing key 'mysecret',
then following steps figure out how to:

1 On server office.example.com:

tcpredirect -k :mysecret office.example.com:8080:www.example.com:8080

2 On server www.example.com:

tcpredirect -k mysecret: www.example.com:8080:www.example.com:80

3 Setup browser using proxy server office.example.com:8080

Traffic flow:

Browser <=> Office(office.example.com:8080) <= ..Secured.. =>
	Site(www.example.com:8080) <=> Proxy(www.example.com:80)

