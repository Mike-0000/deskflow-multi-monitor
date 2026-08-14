import streamDeck from "@elgato/streamdeck";

import { PauseAction } from "./actions/pause";

streamDeck.logger.setLevel("info");

streamDeck.actions.registerAction(new PauseAction());

streamDeck.connect();
