<?php
/**
 * [GENE_FIX:2026-08-24] Repro: Validate::filter() used to mutate the
 * caller's array in place because it skipped SEPARATE_ARRAY before
 * zend_hash_str_update on the shared `data` property array.
 *
 * Symptom on PHP debug builds / Swoole:
 *   _zend_hash_str_add_or_update_i -> HT_ASSERT_RC1(ht) aborts the worker
 *   (GC_REFCOUNT(ht) == 1 fails because the property and the caller's
 *   variable share the same HashTable, refcount >= 2).
 *
 * Symptom on release builds (observable here):
 *   The caller's $input is mutated even though PHP copy-on-write semantics
 *   require it to stay untouched once the validate object owns a separate
 *   copy. After the fix, $input must remain "  captcha  " (untrimmed).
 */

$v = new \Gene\Validate();

$input = ['captcha' => '  captcha  ', 'name' => '  bob  '];
// Keep a reference so the array is shared (refcount >= 2) — mirrors the
// real-world case where the caller passes $_POST / request()->post() and
// keeps using it afterwards.
$alias = $input;

$v->init($input);
$v->name('captcha')->filter('trim');

echo "caller input[captcha] : [", $input['captcha'], "]\n";
echo "alias   input[captcha] : [", $alias['captcha'], "]\n";

// The caller's array must be untouched (COW). Before the fix, filter()
// mutated it in place -> "captcha"; after the fix it stays "  captcha  ".
$ok = ($input['captcha'] === '  captcha  ')
   && ($alias['captcha'] === '  captcha  ');
echo $ok ? "PASS: caller array untouched (COW respected).\n"
         : "FAIL: caller array was mutated in place (COW violation).\n";
exit($ok ? 0 : 1);
