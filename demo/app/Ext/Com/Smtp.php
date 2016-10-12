<?php
namespace Ext\Com;
/**
 * Smtp
 */
class Smtp
{

    public $smtpPort;
    public $timeOut;
    public $hostName;
    public $logFile;
    public $relayHost;
    public $debug;
    public $auth;
    public $user;
    public $pass;
    private $_sock = NULL;

    function __construct($relayHost, $smtpPort, $auth, $user, $pass)
    {
        $this->debug = TRUE;
        $this->smtpPort = $smtpPort;
        $this->relayHost = $relayHost;
        //is used in fsockopen()
        $this->timeOut = 30;
        //auth
        $this->auth = $auth;
        $this->user = $user;
        $this->pass = $pass;
        //is used in HELO command
        $this->hostName = "localhost";
        $this->logFile = "";
        $this->_sock = TRUE;
    }

    /**
     *  Main Function
     */
    function sendMail($to, $from, $subject, $body, $mailType, $cc = NULL, $bcc = NULL, $additionalHeaders = NULL)
    {
        $mailFrom = $this->getAddress($this->stripComment($from));
        $body = preg_replace("/(^|(\r\n))(\.)/", "\1.\3", $body);
        $header = "MIME-Version:1.0\r\n";
        if ($mailType == 'HTML') {
            $header .= "Content-Type:text/html\r\n";
        }
        $header .= 'To: ' . $to . "\r\n";
        if ($cc != "") {
            $header .= "Cc: " . $cc . "\r\n";
        }
        $header .= "From: $from<" . $from . ">\r\n";
        $header .= "Subject: " . $subject . "\r\n";
        $header .= $additionalHeaders;
        $header .= "Date: " . date("r") . "\r\n";
        $header .= "X-Mailer:By Redhat (PHP/" . phpversion() . ")\r\n";
        list($msec, $sec) = explode(" ", microtime());
        $header .= "Message-ID: <" . date("YmdHis", $sec) . "." . ($msec * 1000000) . "." . $mailFrom . ">\r\n";
        $to = explode(",", $this->stripComment($to));

        if ($cc != NULL) {
            $to = array_merge($to, explode(",", $this->stripComment($cc)));
        }
        if ($bcc != NULL) {
            $to = array_merge($to, explode(",", $this->stripComment($bcc)));
        }
        $sent = TRUE;
        foreach ($to as $rcptTo) {
            $rcptTo = $this->getAddress($rcptTo);
            if (!$this->smtpSockopen($rcptTo)) {
                $this->logWrite("Error: Cannot send email to " . $rcptTo . "\n");
                $sent = FALSE;
                continue;
            }
            if ($this->smtpSend($this->hostName, $mailFrom, $rcptTo, $header, $body)) {
                $this->logWrite("E-mail has been sent to <" . $rcptTo . ">\n");
            } else {
                $this->logWrite("Error: Cannot send email to <" . $rcptTo . ">\n");
                $sent = FALSE;
            }
            fclose($this->_sock);
            $this->logWrite("Disconnected from remote host\n");
        }
        return $sent;
    }

    /* Private Functions */

    function smtpSend($helo, $from, $to, $header, $body = "")
    {
        if (!$this->smtpPutcmd("HELO", $helo)) {
            return $this->smtpError("sending HELO command");
        }

        //auth
        if ($this->auth) {
            if (!$this->smtpPutcmd("AUTH LOGIN", base64_encode($this->user))) {
                return $this->smtpError("sending HELO command");
            }
            if (!$this->smtpPutcmd("", base64_encode($this->pass))) {
                return $this->smtpError("sending HELO command");
            }
        }
        if (!$this->smtpPutcmd("MAIL", "FROM:<" . $from . ">")) {
            return $this->smtpError("sending MAIL FROM command");
        }
        if (!$this->smtpPutcmd("RCPT", "TO:<" . $to . ">")) {
            return $this->smtpError("sending RCPT TO command");
        }
        if (!$this->smtpPutcmd("DATA")) {
            return $this->smtpError("sending DATA command");
        }
        if (!$this->smtpMessage($header, $body)) {
            return $this->smtpError("sending message");
        }
        if (!$this->smtpEom()) {
            return $this->smtpError("sending <CR><LF>.<CR><LF> [EOM]");
        }
        if (!$this->smtpPutcmd("QUIT")) {
            return $this->smtpError("sending QUIT command");
        }
        return TRUE;
    }

    function smtpSockopen($address)
    {
        if ($this->relayHost == "") {
            return $this->smtpSockopenMx($address);
        } else {
            return $this->smtpSockopenRelay();
        }
    }

    function smtpSockopenRelay()
    {
        $this->logWrite("Trying to " . $this->relayHost . ":" . $this->smtpPort . "\n");
        $this->_sock = @fsockopen($this->relayHost, $this->smtpPort, $errno, $errstr, $this->timeOut);
        if (!($this->_sock && $this->smtpOk())) {
            $this->logWrite("Error: Cannot connenct to relay host " . $this->relayHost . "\n");
            $this->logWrite("Error: " . $errstr . " (" . $errno . ")\n");
            return FALSE;
        }
        $this->logWrite("Connected to relay host " . $this->relayHost . "\n");
        return TRUE;
    }

    function smtpSockopenMx($address)
    {
        $domain = preg_replace("/^.+@([^@]+)$/", "\1", $address);
        if (!@getmxrr($domain, $MXHOSTS)) {
            $this->logWrite("Error: Cannot resolve MX \"" . $domain . "\"\n");
            return FALSE;
        }
        foreach ($MXHOSTS as $host) {
            $this->logWrite("Trying to " . $host . ":" . $this->smtpPort . "\n");
            $this->_sock = @fsockopen($host, $this->smtpPort, $errno, $errstr, $this->timeOut);
            if (!($this->_sock && $this->smtpOk())) {
                $this->logWrite("Warning: Cannot connect to mx host " . $host . "\n");
                $this->logWrite("Error: " . $errstr . " (" . $errno . ")\n");
                continue;
            }
            $this->logWrite("Connected to mx host " . $host . "\n");
            return TRUE;
        }
        $this->logWrite("Error: Cannot connect to any mx hosts (" . implode(", ", $MXHOSTS) . ")\n");
        return FALSE;
    }

    function smtpMessage($header, $body)
    {
        fputs($this->_sock, $header . "\r\n" . $body);
        $this->smtpDebug("> " . str_replace("\r\n", "\n" . "> ", $header . "\n> " . $body . "\n> "));
        return TRUE;
    }

    function smtpEom()
    {
        fputs($this->_sock, "\r\n.\r\n");
        $this->smtpDebug(". [EOM]\n");
        return $this->smtpOk();
    }

    function smtpOk()
    {
        $response = str_replace("\r\n", "", fgets($this->_sock, 512));
        $this->smtpDebug($response . "\n");
        if (!preg_match("/^[23]/", $response)) {
            fputs($this->_sock, "QUIT\r\n");
            fgets($this->_sock, 512);
            $this->logWrite("Error: Remote host returned \"" . $response . "\"\n");
            return FALSE;
        }
        return TRUE;
    }

    function smtpPutcmd($cmd, $arg = "")
    {
        if ($arg != "") {
            if ($cmd == "") {
                $cmd = $arg;
            } else {
                $cmd = $cmd . " " . $arg;
            }
        }
        fputs($this->_sock, $cmd . "\r\n");
        $this->smtpDebug("> " . $cmd . "\n");
        return $this->smtpOk();
    }

    function smtpError($string)
    {
        $this->logWrite("Error: Error occurred while " . $string . ".\n");
        return FALSE;
    }

    function logWrite($message)
    {
        $this->smtpDebug($message);
        if ($this->logFile == "") {
            return TRUE;
        }
        $message = date("M d H:i:s ") . get_current_user() . "[" . getmypid() . "]: " . $message;
        if (!@file_exists($this->logFile) || !($fp = @fopen($this->logFile, "a"))) {
            $this->smtpDebug("Warning: Cannot open log file \"" . $this->logFile . "\"\n");
            return FALSE;
        }
        flock($fp, LOCK_EX);
        fputs($fp, $message);
        fclose($fp);
        return TRUE;
    }

    function stripComment($address)
    {
        $comment = "/\([^()]*\)/";
        while (preg_match($comment, $address)) {
            $address = preg_replace($comment, "", $address);
        }
        return $address;
    }

    function getAddress($address)
    {
        $address = preg_replace("/([ \t\r\n])+/", "", $address);
        $address = preg_replace("/^.*<(.+)>.*$/", "\1", $address);
        return $address;
    }

    function smtpDebug($message)
    {
        if ($this->debug) {
            echo $message;
        }
    }

}
