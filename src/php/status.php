
<!--  Simple file for showing the status of the RaSCSI process. Intended to be loaded as a frame in a larger page -->
<!--  Copyright (c) 2020 akuker -->
<!--  Distributed under the BSD-3 Clause License -->

<!-- Note: the origina rascsi-php project was under MIT license.-->

<!DOCTYPE html>
<html>

<script type="text/javascript">
    window.onload = setupRefresh;
 
    function setupRefresh() {
      setTimeout("refreshPage();", 30000); // milliseconds
    }
    function refreshPage() {
       window.location = location.href;
    }
</script>

<?php 

// Blatently copied from stack overflow:
//     https://stackoverflow.com/questions/53695187/php-function-that-shows-status-of-systemctl-service
    $output = shell_exec("systemctl is-active rascsi");
    if (trim($output) == "active") {
	    $color='green';
	    $text='Running';
    }
    else{
	    $color='red';
	    $text='Stopped';
    }

    echo '<body style="background-color: '.$color.';">';
    echo '<table width="100%" height=100% style="position: absolute; top: 0; bottom: 0; left: 0; right: 0; background-color:'.$color.'">';
    echo '<tr style:"height: 100%">';
    echo '<td style="color: white; background-color: '.$color.'; text-align: center; vertical-align: center; font-family: Arial, Helvetica, sans-serif;">'.$text.' '.date("h:i:sa").'</td>';
    echo '</tr>';
    echo '</table>';
?>
   </body>
</html>
