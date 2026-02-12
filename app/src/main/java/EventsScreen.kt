package com.example.steam_ic_android_app.ui

import androidx.compose.runtime.Composable
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

data class EventClip(
    val id: String,
    val timestamp: String,
    val durationSeconds: Int,
    val reason: String
)

@Composable
fun EventsScreen(
    clips: List<EventClip>,
    onClipClick: (EventClip) -> Unit
) {
    LazyColumn(
        modifier = Modifier
            .fillMaxSize()
            .padding(16.dp)
    ) {
        items(clips) { clip ->
            Card(
                modifier = Modifier
                    .fillMaxWidth()
                    .padding(vertical = 8.dp)
                    .clickable { onClipClick(clip) }
            ) {
                Row(
                    modifier = Modifier.padding(12.dp),
                    verticalAlignment = Alignment.CenterVertically
                ) {
                    Box(
                        modifier = Modifier
                            .size(64.dp)
                            .background(Color.DarkGray, RoundedCornerShape(8.dp)),
                        contentAlignment = Alignment.Center
                    ) {
                        Text("Preview", color = Color.White, fontSize = 10.sp)
                    }

                    Spacer(Modifier.width(12.dp))

                    Column(modifier = Modifier.weight(1f)) {
                        Text(clip.timestamp, style = MaterialTheme.typography.titleMedium)
                        Text(clip.reason, style = MaterialTheme.typography.bodySmall)
                    }

                    Text("${clip.durationSeconds}s", style = MaterialTheme.typography.bodyMedium)
                }
            }
        }
    }
}
