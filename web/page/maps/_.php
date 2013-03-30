<?php

import_lib('Pager');

$like = $db->real_escape_string($hierarchy[2]);

$pager = new Pager($hierarchy[1] - 1, $shared['max_rows'], "SELECT `id`, `name`, `record`, `record_holder` FROM `map` WHERE `name` LIKE '%$like%' ORDER BY `name`");

$maps = '';
$rows = $pager->getRows();
foreach ($rows as $row) {
    $maps .= '<tr><td>' . format_map($row['name']) . '</td><td>' . format_player($row['record_holder'], $row['id']) . '</td><td>' . format_time($row['record']) . '</td></tr>';
}

?>
<p>
Records below are the best records recorded by the bot, not necessarily actual records.
</p>
<p>
<form action="<?= url('maps'); ?>" method="POST">
<input type="text" name="name" value="<?= $like; ?>" />
<input type="submit" name="submit" value="Search">
</form>
</p>
<?= format_pages(1, $pager); ?>
<table>
    <tr><th>Map</th><th>Record holder</th><th>Record</th></tr>
    <?= $maps; ?>
</table>
