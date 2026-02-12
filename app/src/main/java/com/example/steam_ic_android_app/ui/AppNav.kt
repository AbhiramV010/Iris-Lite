package com.example.steam_ic_android_app.ui

import androidx.compose.runtime.Composable
import androidx.navigation.NavHostController
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.rememberNavController

@Composable
fun AppNav() {
    val navController = rememberNavController()

    NavHost(
        navController = navController,
        startDestination = "live"
    ) {
        composable("live") {
            LiveViewScreen(
                isZoneCovered = true,
                isRecording = false,
                onEventsClick = { navController.navigate("events") }
            )
        }

        composable("events") {
            EventsScreen(
                clips = sampleClips(),
                onClipClick = { /* later */ }
            )
        }
    }
}

fun sampleClips(): List<EventClip> = listOf(
    EventClip("1", "Today 4:12 PM", 12, "Motion detected"),
    EventClip("2", "Today 3:58 PM", 8, "Zone uncovered"),
    EventClip("3", "Today 2:21 PM", 5, "Loud noise detected")
)
