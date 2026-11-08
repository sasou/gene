# Rewrites git commit messages on stdin -> stdout (used with git filter-branch --msg-filter).
$text = [Console]::In.ReadToEnd()
$protected = New-Object System.Collections.Generic.List[string]

$text = [regex]::Replace($text, 'gene_webscan\w*', {
    param($m)
    $script:protected.Add($m.Value) | Out-Null
    $idx = $script:protected.Count - 1
    return ([char]0) + "WEBSCAN$idx" + ([char]0)
})

$text = [regex]::Replace($text, 'MariaDB 1064 \(mchat\s*\r?\n\s*Purview::lists\)', 'MariaDB 1064 (demo raw in()/where() with embedded connector)')
$text = [regex]::Replace($text, 'MariaDB 1064 \(mchat\s+Purview::lists\)', 'MariaDB 1064 (demo raw in()/where() with embedded connector)')
$text = [regex]::Replace($text, 'mchat\s+Purview::lists', 'demo ORM connector-carrying fragment')
$text = [regex]::Replace($text, 'Purview::lists', 'ORM lists() with embedded connector')

$text = [regex]::Replace($text, '(?i)exchange/mchat/\s*\r?\n\s*gene_web', '业务A/业务B/业务C')
$text = [regex]::Replace($text, '(?i)exchange/mchat/', '业务A/业务B/')
$text = [regex]::Replace($text, '(?i)\bexchange\b', '业务A')
$text = [regex]::Replace($text, '(?i)\bmchat\b', '业务B')

$text = [regex]::Replace($text, '(?i)\bgene_web\b', 'demo')
$text = $text.Replace('GENE_WEB', 'DEMO_ROOT')
$text = $text.Replace('wait_for_gene_web', 'wait_for_demo_web')
$text = $text.Replace('gene-web', 'demo-web')

$text = [regex]::Replace($text, '(?i)F:[/\\]codeup[/\\](?:gene_web|demo)[/\\]', '<repo>/demo/')
$text = [regex]::Replace($text, '(?i)F:[/\\]codeup[/\\]', '<repo>/')

$text = $text.Replace('root/123456', 'local dev credentials')
$text = $text.Replace('按线上 gene_web 注册顺序', '按 demo 应用注册顺序')
$text = [regex]::Replace($text, '(?i)drop external gene_web dependency', 'drop external web-app dependency')
$text = [regex]::Replace($text, '(?i)drop external demo dependency', 'drop external web-app dependency')
$text = [regex]::Replace($text, '(?i)for gene_web DB/Redis gate', 'for demo DB/Redis gate')

for ($i = 0; $i -lt $protected.Count; $i++) {
    $marker = ([char]0) + "WEBSCAN$i" + ([char]0)
    $text = $text.Replace($marker, $protected[$i])
}

[Console]::Out.Write($text)
