<?php
// Repro: name('x')->rule_*() used to warn "Please call the name method in the
// first place." because name() set KEY but not FIELD (read by getFieldVal).
$v = new \Gene\Validate();
$v->init(['email' => 'a@b.com', 'age' => 25]);

var_dump($v->name('email')->rule_email());   // expect true, no warning
var_dump($v->name('age')->rule_int());       // expect true, no warning
var_dump($v->name('missing')->rule_email()); // expect false, no warning

// fluent config path must be unchanged
$v2 = new \Gene\Validate();
$v2->init(['email' => 'bad', 'age' => '30']);
$ok = $v2->name('email')->email()->msg('bad email')
        ->name('age')->int()->msg('bad age')
        ->valid();
var_dump($ok);            // expect false
var_dump($v2->error());   // expect 'bad email'
echo "DONE\n";
