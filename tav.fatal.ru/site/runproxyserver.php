<?php
header('Content-Type: text/plain');

/* Разрешить скрипту зависнуть в ожидании соединений. */
set_time_limit (0);

/* Включить неявную очистку вывода, и мы увидим всё получаемое
 * по мере поступления. */
ob_implicit_flush ();

$address = '192.168.0.1';
$port = 8080;

if (($sock = socket_create (AF_INET, SOCK_STREAM, 0)) < 0) {
    exit("socket_create() failed: reason: " . socket_strerror ($sock) . "\n");
}

if (($ret = socket_bind ($sock, $address, $port)) < 0) {
    exit("socket_bind() failed: reason: " . socket_strerror ($ret) . "\n");
}

if (($ret = socket_listen ($sock, 5)) < 0) {
    exit("socket_listen() failed: reason: " . socket_strerror ($ret) . "\n");
}

/*echo 'Прокси-сервер запущен

';*/
echo "ok\r\n";

do {
    if (($msgsock = socket_accept($sock)) < 0) {
        echo "socket_accept() failed: reason: " . socket_strerror ($msgsock) . "\n";
        break;
    }
    /* Отправить инструкции. */
/*    $msg = "\nWelcome to the PHP Test Server. \n" .
        "To quit, type 'quit'. To shut down the server type 'shutdown'.\n";
    socket_write($msgsock, $msg, strlen($msg));*/

//	socket_set_blocking($msgsock,FALSE);

	$buf='';

    do {
        if (FALSE === ($ret = socket_read ($msgsock, 2048))) {
            //echo "socket_read() failed: reason: " . socket_strerror(socket_last_error($msgsock)) . "\n";
            break;
        }
        $buf.=$ret;
        if (preg_match('/(\r?\n){2}/',$buf,$r,PREG_OFFSET_CAPTURE))
        {
        	if (preg_match('/^POST/',$buf))
        	{
        		$headers=substr($buf,0,$r[0][1]);
        		if (preg_match('/^Content-Length: (\d+)/m',$headers,$r))
        		{
        			$left=strlen($headers)+strlen($r[0][0])+(int)$r[1]-strlen($buf);
        			while ($left>0)
        			{
        				if (FALSE === ($ret = socket_read ($msgsock, 2048))) break 2;
	        			$buf.=$ret;
	        			$left-=strlen($ret);
        			}
        		}
        	}

        	$buf=preg_replace('/^Keep-Alive: .*\r?\n/m','',preg_replace('/^Proxy-Connection: .*\r?\n/m',"Connection: Close\r\n",$buf));

	        echo "Request:\r\n$buf\r\n";
    	    if (preg_match('/^Host: ([^\r\n]+)/m',$buf,$r))
    	    {
				$fp = @fsockopen($r[1], 80, $errno, $errstr);
				if (!$fp) {echo "\r\nCan't open $r[1]. Reason: ($errno) $errstr\r\n"; break;}
				fwrite($fp, $buf);
				socket_set_blocking($fp,FALSE);
				echo 'Loading';
				$totalLen=0;
				while(!feof($fp))
				{
		    	    $talkback = fread($fp, 2048);
		    	    $totalLen += strlen($talkback);
		    	    while ($totalLen>0) {echo '.'; $totalLen-=1024;}
		        	socket_write($msgsock, $talkback, strlen($talkback));

		        	if (connection_aborted()) exit;
				}
	    	    fclose($fp);
	    	    echo "ok\r\n\n";
    	    }
//		    	    $talkback = "Content-Type: text/plain\n\nTEXT";
//		        	socket_write($msgsock, $talkback, strlen ($talkback));
	        //$buf='';
	        break;
        }
    } while(true);
    socket_close($msgsock);
} while(true);

socket_close($sock);
?>