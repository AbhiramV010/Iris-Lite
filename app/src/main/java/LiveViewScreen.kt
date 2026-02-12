package com.example.steam_ic_android_app.ui

import androidx.compose.runtime.Composable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.background
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.ui.draw.clip


@Composable
fun LiveViewScreen(
    isZoneCovered: Boolean,
    isRecording: Boolean,
    onEventsClick: () -> Unit
) {
    Column(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp)
    ) {
        Box(
            modifier = Modifier
                .fillMaxWidth()
                .weight(1f)
                .clip(RoundedCornerShape(16.dp))
                .background(Color.Black),
            contentAlignment = Alignment.Center
        ) {
            Text("Live Camera Feed", color = Color.White)
        }

        Spacer(Modifier.height(16.dp))

        Row(
            modifier = Modifier.fillMaxWidth(),
            horizontalArrangement = Arrangement.SpaceBetween,
            verticalAlignment = Alignment.CenterVertically
        ) {
            Column {
                Text("Zone status", style = MaterialTheme.typography.labelMedium)
                Text(
                    text = if (isZoneCovered) "Covered" else "Uncovered",
                    color = if (isZoneCovered) Color(0xFF4CAF50) else Color(0xFFF44336),
                    style = MaterialTheme.typography.titleMedium
                )
            }

            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(
                    modifier = Modifier
                        .size(12.dp)
                        .background(
                            if (isRecording) Color.Red else Color.Gray,
                            shape = CircleShape
                        )
                )
                Spacer(Modifier.width(8.dp))
                Text(if (isRecording) "Recording" else "Idle")
            }

            Button(onClick = onEventsClick) {
                Text("Events")
            }
        }
    }
}
