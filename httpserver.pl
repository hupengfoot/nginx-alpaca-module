#!/usr/bin/perl

{
	package MyWebServer;

	use HTTP::Server::Simple::CGI;
	use base qw(HTTP::Server::Simple::CGI);

	my %dispatch = (
		'/hello' => \&resp_hello,
	'/hupeng' => \&sendmessage,
# ...
	);

	sub handle_request {
		my $self = shift;
		my $cgi  = shift;

		my $path = $cgi->path_info();
		my $handler = $dispatch{$path};

		if (ref($handler) eq "CODE") {
			print "HTTP/1.0 200 OK\r\n";
			$handler->($cgi);

		} else {
			print "HTTP/1.0 404 Not found\r\n";
			print $cgi->header,
			$cgi->start_html('Not found'),
			$cgi->h1('Not found'),
			$cgi->end_html;
		}
	}

	sub resp_hello {
		my $cgi  = shift;   # CGI.pm object
		return if !ref $cgi;

		my $who = $cgi->param('name');

		print $cgi->header,
		$cgi->start_html("Hello"),
		$cgi->h1("Hello $who!"),
		$cgi->end_html;
	}

	sub sendmessage {
		my $content;
		if ($ENV{'REQUEST_METHOD'} eq "POST")
		{
			read(STDIN, $content, $ENV{'CONTENT_LENGTH'});
		}
		else {
			$content = $ENV{'QUERY_STRING'};
	    	}
		my $cgi  = shift;   # CGI.pm object
		my @mobile_num = ("13917658422");
		#$content = $cgi->path_info();
		foreach my $num (@mobile_num) 
		{
			my $url = "http://211.136.163.68:8000/httpserver?enterpriseid=95102&accountid=000&pswd=z5PgZ4&mobs=$num&msg=$content";
			system("wget -o /dev/null \"$url\"");
		}
		return if !ref $cgi;

		my $who = $cgi->param('name');

		print $cgi->header,
		$cgi->start_html("Hello"),
		$cgi->h1("Hello $who!"),
		$cgi->end_html;
	}

} 

# start the server on port 8080
my $pid = MyWebServer->new(8088)->background();
print "Use 'kill $pid' to stop server.\n";
